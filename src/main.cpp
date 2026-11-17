#include "AtlasNet/ControlPanel/ControlPanelClient.hpp"
#include "MapModel.hpp"
#include <chrono>
#include <deque>
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/canvas.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/flexbox_config.hpp>
#include <ftxui/screen/pixel.hpp>
#include <functional>
#include <iostream>
#include <mutex>
#include <optional>
#include <sstream>
#include <thread>

using namespace AtlasNetTUI;
using namespace ftxui;
using Clock = std::chrono::steady_clock;
int main(int argc, char** argv)
{
  std::string url = "ws://127.0.0.1:8080";
  for (int i = 1; i < argc; ++i)
  {
    const std::string argument = argv[i];
    if (argument == "--help")
    {
      std::cout << "atlasnet-tui [--connect ws://host:port]\nDrag: pan | "
                   "Wheel: zoom | Hover: details | Tab/Space: layers | "
                   "r: reconnect | q: quit\n";
      return 0;
    }
    if (argument == "--connect" && i + 1 < argc)
      url = argv[++i];
    else
    {
      std::cerr << "Unknown or incomplete option: " << argument << '\n';
      return 2;
    }
  }
  auto screen = ScreenInteractive::Fullscreen();
  AtlasNet::ControlPanel::ControlPanelClient client;
  Camera camera;
  Layers layers;
  EntityStream stream;
  JSON heuristic, bounds;
  std::string plane = "XY", status = "Connecting to " + url;
  bool connected = false, busy = false, dragging = false;
  bool heuristicPending = false, boundsPending = false;
  Point lastMouse;
  Box mapBox;
  std::optional<Mouse> mousePosition;
  std::array<std::string, 3> hoverDetails;
  Bounds subscribedBounds{};
  auto nextRequest = Clock::now(), nextMetadata = Clock::now();
  std::mutex queueMutex;
  std::deque<std::function<void()>> queue;
  const auto enqueue = [&](std::function<void()> job)
  {
    {
      std::scoped_lock lock(queueMutex);
      queue.push_back(std::move(job));
    }
    screen.PostEvent(Event::Custom);
  };
  const auto responseError = [&](const JSON& response)
  {
    if (!response.contains("error"))
      return false;
    status = response["error"].value("message", "Management request failed");
    return true;
  };
  const auto connect = [&]
  {
    client.Connect(url,
                   [&](bool value, std::string message)
                   {
                     enqueue(
                         [&, value, message = std::move(message)]
                         {
                           connected = value;
                           status = message;
                           busy = heuristicPending = boundsPending = false;
                           stream.Clear();
                           heuristic = bounds = nullptr;
                           nextMetadata = Clock::now();
                         });
                   });
  };
  const auto finishSubscription = [&](JSON response)
  {
    enqueue(
        [&, response = std::move(response)]
        {
          busy = false;
          if (responseError(response))
          {
            nextRequest = Clock::now() + std::chrono::seconds(1);
            if (response["error"].value("code", "") == "subscription_timeout")
              stream.Clear();
            return;
          }
          stream.Snapshot(response.at("result"));
          status = "Connected to " + url;
        });
  };
  const auto tick = [&]
  {
    std::deque<std::function<void()>> jobs;
    {
      std::scoped_lock lock(queueMutex);
      jobs.swap(queue);
    }
    for (auto& job : jobs)
    {
      try
      {
        job();
      }
      catch (const std::exception& e)
      {
        status = e.what();
        stream.inconsistent = true;
      }
    }
    if (!connected)
      return;
    const auto now = Clock::now();
    // The Controller can replace its initial unbounded region after clients join.
    if (!heuristicPending && !boundsPending && now >= nextMetadata)
    {
      heuristicPending = boundsPending = true;
      client.Request(
          std::string(AtlasNet::ControlPanel::GetHeuristic), JSON::object(),
          [&](JSON response)
          {
            enqueue(
                [&, response = std::move(response)]
                {
                  heuristicPending = false;
                  const auto interval = heuristic.is_null() && response.contains("error")
                                            ? std::chrono::seconds(1)
                                            : std::chrono::seconds(5);
                  nextMetadata = Clock::now() + interval;
                  if (responseError(response))
                    return;
                  heuristic = response.at("result");
                  plane = heuristic.value("dimension", 2) == 3
                              ? "XY"
                              : heuristic.value("plane", "XY");
                });
          });
      client.Request(std::string(AtlasNet::ControlPanel::GetSimulationBounds),
                     JSON::object(),
                     [&](JSON response)
                     {
                       enqueue(
                           [&, response = std::move(response)]
                           {
                             boundsPending = false;
                             if (responseError(response))
                               return;
                             const bool firstBounds = bounds.is_null();
                             bounds = response.at("result");
                             if (firstBounds && bounds.at("min")[0].is_number() &&
                                 bounds.at("max")[0].is_number() &&
                                 bounds.at("min")[1].is_number() &&
                                 bounds.at("max")[1].is_number())
                             {
                               double x0 = bounds["min"][0],
                                      x1 = bounds["max"][0],
                                      y0 = bounds["min"][1],
                                      y1 = bounds["max"][1];
                               camera.center = {(x0 + x1) / 2, (y0 + y1) / 2};
                               camera.unitsPerPixel =
                                   std::max({(x1 - x0) / camera.width,
                                             (y1 - y0) / camera.height, 1e-4}) *
                                   1.2;
                             }
                           });
                     });
    }
    if (busy || now < nextRequest)
      return;
    if (!layers.entities)
    {
      if (stream.id.empty())
        return;
      busy = true;
      client.Unsubscribe(stream.id,
                         [&](JSON response)
                         {
                           enqueue(
                               [&, response = std::move(response)]
                               {
                                 busy = false;
                                 if (!responseError(response))
                                   stream.Clear();
                               });
                         });
      return;
    }
    if (heuristic.is_null())
      return;
    if (!stream.id.empty() && !stream.inconsistent &&
        subscribedBounds.Contains(camera.ViewBounds()))
      return;
    const auto requested = camera.ViewBounds(1.4);
    auto parameters = SubscriptionParameters(requested, plane);
    busy = true;
    subscribedBounds = requested;
    nextRequest = now + std::chrono::milliseconds(120);
    if (stream.id.empty())
    {
      client.Subscribe(
          std::string(AtlasNet::ControlPanel::EntityPositions), parameters,
          [&](JSON update)
          {
            enqueue([&, update = std::move(update)] { stream.Update(update); });
          },
          finishSubscription);
    }
    else
      client.UpdateSubscription(stream.id, parameters, finishSubscription);
  };
  auto regionCheck = Checkbox("Heuristic regions", &layers.heuristic);
  auto entityCheck = Checkbox("Entity locations", &layers.entities);
  auto boundsCheck = Checkbox("Simulation bounds", &layers.bounds);
  auto controls =
      Container::Horizontal({regionCheck, entityCheck, boundsCheck});
  auto map = Renderer(
      [&]
      {
        camera.width = std::max(2, (mapBox.x_max - mapBox.x_min + 1) * 2);
        camera.height = std::max(4, (mapBox.y_max - mapBox.y_min + 1) * 4);
        Canvas image(camera.width, camera.height);
        hoverDetails = {};
        std::optional<std::pair<int, int>> hoverCell;
        if (mousePosition && mapBox.Contain(mousePosition->x, mousePosition->y))
          hoverCell.emplace(mousePosition->x - mapBox.x_min,
                            mousePosition->y - mapBox.y_min);
        const auto draw = [&](const JSON& region, Color color, unsigned dim)
        {
          for (const auto& line :
               RegionSegments(region, camera.ViewBounds(), dim))
          {
            const auto a = camera.WorldToScreen(line.a),
                       b = camera.WorldToScreen(line.b);
            image.DrawPointLine(
                std::clamp(int(std::lround(a.x)), 0, camera.width - 1),
                std::clamp(int(std::lround(a.y)), 0, camera.height - 1),
                std::clamp(int(std::lround(b.x)), 0, camera.width - 1),
                std::clamp(int(std::lround(b.y)), 0, camera.height - 1), color);
          }
        };
        try
        {
          if (layers.heuristic && !heuristic.is_null())
          {
            const unsigned dimension = heuristic.at("dimension");
            for (const auto& region : heuristic.at("regions"))
              draw(region, Color::RGB(255, 155, 40), dimension);
            if (hoverCell)
            {
              const auto point = camera.ScreenToWorld(
                  {hoverCell->first * 2.0 + 1, hoverCell->second * 4.0 + 2});
              for (const auto& region : heuristic.at("regions"))
              {
                if (!RegionContains(region, point, dimension))
                  continue;
                const auto owner = region.value("owner", JSON::object());
                hoverDetails[0] =
                    "Region ID: " + region.at("id").dump() +
                    " | Shard ID: " + owner.value("shard", "unassigned");
                break;
              }
            }
          }
          if (layers.bounds && !bounds.is_null())
          {
            auto region = bounds;
            region["planes"] = JSON::array();
            draw(region, Color::GreenLight, bounds.at("dimension"));
          }
          if (layers.entities)
          {
            std::vector<std::pair<int, int>> clientMarkers;
            const std::pair<const std::string, MapEntity>* hoveredEntity = nullptr;
            for (const auto& entry : stream.entities)
            {
              const auto& entity = entry.second;
              auto p = camera.WorldToScreen(Project(entity.position, plane));
              if (p.x < 0 || p.x >= camera.width || p.y < 0 ||
                  p.y >= camera.height)
                continue;
              const int x = int(p.x), y = int(p.y);
              if (hoverCell == std::pair{x / 2, y / 4} &&
                  (!hoveredEntity || entity.isClient))
                hoveredEntity = &entry;
              if (entity.isClient)
                clientMarkers.emplace_back(x, y);
              else
                image.DrawPoint(x, y, true, Color::BlueLight);
            }
            for (const auto& [x, y] : clientMarkers)
            {
              image.DrawText(x, y, "△", [](Pixel& pixel)
              {
                pixel.foreground_color = Color::Green;
                pixel.bold = true;
              });
            }
            if (hoveredEntity)
            {
              const auto& [id, entity] = *hoveredEntity;
              hoverDetails[1] = "Entity ID: " + id;
              if (entity.isClient)
                hoverDetails[1] +=
                    " | Client ID: " +
                    (entity.clientId.empty() ? "unknown" : entity.clientId);
              std::ostringstream details;
              const auto& [x, y, z] = entity.position;
              details << "Current shard ID: "
                      << (entity.shardId.empty() ? "unknown" : entity.shardId)
                      << " | Position: (" << x << ", " << y << ", " << z << ')';
              hoverDetails[2] = details.str();
            }
          }
        }
        catch (const std::exception& e)
        {
          status = e.what();
        }
        return canvas(std::move(image)) | flex | reflect(mapBox);
      });
  auto mapEvents = CatchEvent(
      map,
      [&](Event event)
      {
        if (!event.is_mouse())
          return false;
        const auto mouse = event.mouse();
        const bool inside = mapBox.Contain(mouse.x, mouse.y);
        Point cursor{double((mouse.x - mapBox.x_min) * 2),
                     double((mouse.y - mapBox.y_min) * 4)};
        if (mouse.motion == Mouse::Released)
        {
          const bool was = dragging;
          dragging = false;
          return was;
        }
        if (!inside && !dragging)
          return false;
        if (mouse.button == Mouse::WheelUp || mouse.button == Mouse::WheelDown)
        {
          camera.Zoom(cursor, mouse.button == Mouse::WheelUp ? 1.2 : 1 / 1.2);
          return true;
        }
        if (mouse.motion == Mouse::Pressed && mouse.button == Mouse::Left)
        {
          dragging = true;
          lastMouse = cursor;
          return true;
        }
        if (dragging && mouse.motion == Mouse::Moved)
        {
          camera.Pan({cursor.x - lastMouse.x, cursor.y - lastMouse.y});
          lastMouse = cursor;
          return true;
        }
        return false;
      });
  std::vector<std::string> tabs = {"[ Map ]"};
  int selectedTab = 0;
  auto tabBar = Menu(tabs, &selectedTab, MenuOption::Horizontal());
  auto mapPage = Container::Vertical({controls, mapEvents});
  auto pages = Container::Tab({mapPage}, &selectedTab);
  auto container = Container::Vertical({tabBar, pages});
  auto layout = Renderer(
      container,
      [&]
      {
        return vbox(
                   {tabBar->Render(), separator(),
                    flexbox({text("Visibility") | bold, regionCheck->Render(),
                             entityCheck->Render(), boundsCheck->Render()},
                            FlexboxConfig().SetGap(2, 0)),
                    separator(), mapEvents->Render() | flex, separator(),
                    text(hoverDetails[0]), text(hoverDetails[1]),
                    text(hoverDetails[2]),
                    text("Entities: " + std::to_string(stream.entities.size()) +
                         "  " + status) |
                        color(connected ? Color::GreenLight : Color::RedLight),
                    text("Drag pan | Wheel zoom | Tab/Space layers | "
                         "r reconnect | q quit | " +
                         plane) |
                        dim}) |
               border;
      });
  auto root =
      CatchEvent(layout,
                 [&](Event event)
                 {
                   if (event.is_mouse())
                     mousePosition = event.mouse();
                   if (event == Event::Character('q') || event == Event::Escape)
                   {
                     screen.Exit();
                     return true;
                   }
                   if (event == Event::Character('r'))
                   {
                     connect();
                     return true;
                   }
                   if (event == Event::Custom)
                   {
                     tick();
                     return true;
                   }
                   return false;
                 });
  connect();
  std::jthread redraw(
      [&](std::stop_token stop)
      {
        while (!stop.stop_requested())
        {
          std::this_thread::sleep_for(std::chrono::milliseconds(25));
          screen.PostEvent(Event::Custom);
        }
      });
  screen.Loop(root);
  redraw.request_stop();
  redraw.join();
  client.Disconnect();
}

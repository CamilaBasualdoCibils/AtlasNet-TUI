#include "MapModel.hpp"
#include <algorithm>
#include <cmath>
#include <optional>
#include <stdexcept>
namespace AtlasNetTUI
{
bool Bounds::Contains(const Bounds& b) const
{
  return min.x <= b.min.x && min.y <= b.min.y && max.x >= b.max.x &&
         max.y >= b.max.y;
}
Point Camera::WorldToScreen(Point p) const
{
  return {(p.x - center.x) / unitsPerPixel + width / 2.0,
          (center.y - p.y) / unitsPerPixel + height / 2.0};
}
Point Camera::ScreenToWorld(Point p) const
{
  return {center.x + (p.x - width / 2.0) * unitsPerPixel,
          center.y - (p.y - height / 2.0) * unitsPerPixel};
}
void Camera::Pan(Point d)
{
  center.x -= d.x * unitsPerPixel;
  center.y += d.y * unitsPerPixel;
}
void Camera::Zoom(Point cursor, double factor)
{
  if (!std::isfinite(factor) || factor <= 0)
    return;
  auto before = ScreenToWorld(cursor);
  unitsPerPixel = std::clamp(unitsPerPixel / factor, 1e-7, 1e9);
  auto after = ScreenToWorld(cursor);
  center.x += before.x - after.x;
  center.y += before.y - after.y;
}
Bounds Camera::ViewBounds(double margin) const
{
  return {{center.x - width * unitsPerPixel * margin / 2,
           center.y - height * unitsPerPixel * margin / 2},
          {center.x + width * unitsPerPixel * margin / 2,
           center.y + height * unitsPerPixel * margin / 2}};
}
namespace
{
struct Halfspace
{
  double x, y;
  long double offset;
  bool exclusive;

  long double Distance(Point p) const
  {
    return static_cast<long double>(x) * p.x +
           static_cast<long double>(y) * p.y - offset;
  }
};
std::vector<Halfspace> RegionHalfspaces(const JSON& region, unsigned dimension,
                                      double slice)
{
  std::vector<Halfspace> planes;
  for (unsigned axis = 0; axis < dimension; ++axis)
  {
    for (bool upper : {false, true})
    {
      const auto& value = region.at(upper ? "max" : "min").at(axis);
      if (value.is_null())
        continue;
      const double sign = upper ? 1 : -1;
      const double offset = value.get<double>() * sign;
      planes.push_back({axis == 0 ? sign : 0, axis == 1 ? sign : 0,
                        offset - (axis == 2 ? sign * slice : 0), upper});
    }
  }
  for (const auto& p : region.at("planes"))
  {
    const auto n = p.at("normal").get<std::array<double, 3>>();
    const long double z = dimension == 3 ? static_cast<long double>(n[2]) * slice
                                         : 0;
    planes.push_back({n[0], n[1], p.at("offset").get<double>() - z,
                      p.value("exclusive", false)});
  }
  return planes;
}
} // namespace
bool RegionContains(const JSON& region, Point point, unsigned dimension,
                    double slice)
{
  return std::ranges::all_of(
      RegionHalfspaces(region, dimension, slice),
      [point](const Halfspace& plane)
      {
        const auto distance = plane.Distance(point);
        return plane.exclusive ? distance < 0 : distance <= 0;
      });
}
std::vector<Segment> RegionSegments(const JSON& region, const Bounds& view,
                                    unsigned dimension, double slice)
{
  const auto planes = RegionHalfspaces(region, dimension, slice);
  std::vector<Point> polygon = {{view.min.x, view.min.y},
                                {view.max.x, view.min.y},
                                {view.max.x, view.max.y},
                                {view.min.x, view.max.y}};
  for (auto plane : planes)
  {
    if (plane.x == 0 && plane.y == 0 && plane.exclusive && plane.offset == 0)
      return {};
    std::vector<Point> next;
    for (size_t i = 0; i < polygon.size(); ++i)
    {
      auto a = polygon[i], b = polygon[(i + 1) % polygon.size()];
      const auto da = plane.Distance(a), db = plane.Distance(b);
      if (da <= 0)
        next.push_back(a);
      if ((da < 0 && db > 0) || (da > 0 && db < 0))
      {
        double t = da / (da - db);
        next.push_back({a.x + t * (b.x - a.x), a.y + t * (b.y - a.y)});
      }
    }
    polygon = std::move(next);
    if (polygon.empty())
      return {};
  }
  std::vector<Segment> result;
  for (size_t i = 0; i < polygon.size(); ++i)
  {
    const auto a = polygon[i], b = polygon[(i + 1) % polygon.size()];
    for (const auto& p : planes)
    {
      if (p.x == 0 && p.y == 0)
        continue;
      double tolerance = 1e-8 * (1 + std::abs(p.offset) + std::abs(p.x * a.x) +
                                 std::abs(p.y * a.y));
      if (std::abs(p.Distance(a)) <= tolerance &&
          std::abs(p.Distance(b)) <= tolerance)
      {
        result.push_back({a, b});
        break;
      }
    }
  }
  return result;
}
void EntityStream::Clear()
{
  id.clear();
  revision = sequence = 0;
  inconsistent = false;
  entities.clear();
}
void EntityStream::Snapshot(const JSON& value)
{
  auto nextID = value.at("subscriptionId").get<std::string>();
  auto nextRevision = value.at("revision").get<uint64_t>();
  if (id == nextID && (nextRevision < revision ||
                       (nextRevision == revision &&
                        value.at("sequence").get<uint64_t>() < sequence)))
    return;
  std::map<std::string, MapEntity> next;
  for (const auto& entity : value.at("entities"))
    next[entity.at("id").get<std::string>()] =
        {entity.at("position").get<std::array<double, 3>>(),
         entity.value("isClient", false),
         entity.value("shardId", std::string{}),
         entity.value("clientId", std::string{})};
  id = std::move(nextID);
  revision = nextRevision;
  sequence = value.at("sequence").get<uint64_t>();
  entities = std::move(next);
  inconsistent = false;
}
bool EntityStream::Update(const JSON& value)
{
  if (value.at("subscriptionId") != id ||
      value.at("revision").get<uint64_t>() < revision)
    return false;
  const auto serial = value.at("sequence").get<uint64_t>();
  if (value.at("revision") != revision || serial > sequence + 1)
  {
    inconsistent = true;
    return false;
  }
  if (serial <= sequence || inconsistent)
    return false;
  struct Change
  {
    std::string id;
    std::optional<std::array<double, 3>> position;
    std::optional<bool> isClient;
    std::optional<std::string> shardId, clientId;
  };
  std::vector<Change> changes;
  for (const auto& delta : value.at("deltas"))
  {
    const auto key = delta.at("id").get<std::string>();
    const auto type = delta.at("type").get<std::string>();
    if (type == "remove")
      changes.push_back({.id = key});
    else if (type == "add" || type == "move")
    {
      Change change{.id = key,
                    .position = delta.at("position").get<std::array<double, 3>>()};
      if (delta.contains("isClient"))
        change.isClient = delta.at("isClient").get<bool>();
      if (delta.contains("shardId"))
        change.shardId = delta.at("shardId").get<std::string>();
      if (delta.contains("clientId") || change.isClient.has_value())
        change.clientId = delta.value("clientId", std::string{});
      changes.push_back(std::move(change));
    }
    else
      throw std::invalid_argument("Unknown entity delta");
  }
  for (const auto& [key, position, isClient, shardId, clientId] : changes)
  {
    if (position)
    {
      auto& entity = entities[key];
      entity.position = *position;
      if (isClient)
        entity.isClient = *isClient;
      if (shardId)
        entity.shardId = *shardId;
      if (clientId)
        entity.clientId = *clientId;
    }
    else
      entities.erase(key);
  }
  sequence = serial;
  return true;
}
JSON SubscriptionParameters(const Bounds& b, std::string plane)
{
  std::array<double, 3> min{-1e30, -1e30, -1e30}, max{1e30, 1e30, 1e30};
  unsigned a = plane == "YZ" ? 1 : 0, c = plane == "XY" ? 1 : 2;
  min[a] = b.min.x;
  max[a] = b.max.x;
  min[c] = b.min.y;
  max[c] = b.max.y;
  return {{"world", 0}, {"bounds", {{"min", min}, {"max", max}}}};
}
Point Project(const std::array<double, 3>& p, std::string plane)
{
  return plane == "XZ"   ? Point{p[0], p[2]}
         : plane == "YZ" ? Point{p[1], p[2]}
                         : Point{p[0], p[1]};
}
} // namespace AtlasNetTUI

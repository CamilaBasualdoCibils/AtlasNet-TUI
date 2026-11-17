#include "MapModel.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace AtlasNetTUI;
void Check(bool condition)
{
  if (!condition)
    throw std::runtime_error("Map assertion failed");
}
bool Near(double a, double b)
{
  return std::abs(a - b) < 1e-7;
}
int main()
{
  Camera c;
  c.width = 200;
  c.height = 100;
  c.center = {15, -9};
  auto screen = c.WorldToScreen({23, 42});
  auto world = c.ScreenToWorld(screen);
  Check(Near(world.x, 23) && Near(world.y, 42));
  c.Pan({10, -20});
  Check(Near(c.center.x, 10) && Near(c.center.y, -19));
  Point cursor{37, 68};
  auto before = c.ScreenToWorld(cursor);
  c.Zoom(cursor, 2.5);
  auto after = c.ScreenToWorld(cursor);
  Check(Near(before.x, after.x) && Near(before.y, after.y));
  auto b = c.ViewBounds();
  Check(Near(b.max.x - b.min.x, 40) && Near(b.max.y - b.min.y, 20));
  Check(c.ViewBounds(1.4).Contains(b));
  Layers l;
  Check(l.entities && l.heuristic && !l.bounds);
  l.entities = false;
  Check(!l.entities);
  auto params = SubscriptionParameters(b, "XZ");
  Check(Near(params["bounds"]["min"][2], b.min.y));
  JSON region = {
      {"min", {nullptr, nullptr}},
      {"max", {nullptr, nullptr}},
      {"planes", JSON::array({{{"normal", {1, 1, 0}}, {"offset", 0}}})}};
  auto segments = RegionSegments(region, {{-10, -10}, {10, 10}});
  Check(segments.size() == 1);
  Check(Near(segments[0].a.x + segments[0].a.y, 0) &&
        Near(segments[0].b.x + segments[0].b.y, 0));
  Check(RegionContains(region, {-2, 1}) && !RegionContains(region, {1, 2}));
  Check(RegionContains(region, {1, -1}));
  region["planes"][0]["exclusive"] = true;
  Check(!RegionContains(region, {1, -1}));
  region["planes"] = JSON::array();
  Check(RegionSegments(region, {{-10, -10}, {10, 10}}).empty());
  Check(RegionContains(region, {0, 0}));
  region["min"].push_back(-2);
  region["max"].push_back(0);
  Check(RegionContains(region, {0, 0}, 3, -2));
  Check(!RegionContains(region, {0, 0}, 3));
  EntityStream stream;
  stream.Snapshot({{"subscriptionId", "sub"},
                   {"revision", 2},
                   {"sequence", 0},
                   {"entities", JSON::array({{{"id", "a"},
                                              {"position", {1, 2, 3}},
                                              {"isClient", true},
                                              {"shardId", "old-shard"},
                                              {"clientId", "42"}}})}});
  Check(stream.entities.at("a").isClient &&
        stream.entities.at("a").shardId == "old-shard" &&
        stream.entities.at("a").clientId == "42");
  auto update = JSON{
      {"subscriptionId", "sub"},
      {"revision", 2},
      {"sequence", 1},
      {"deltas",
       JSON::array({{{"id", "a"}, {"type", "move"}, {"position", {3, 4, 5}},
                     {"shardId", "new-shard"}},
                    {{"id", "b"}, {"type", "add"}, {"position", {0, 0, 0}}},
                    {{"id", "c"},
                     {"type", "add"},
                     {"position", {1, 1, 1}},
                     {"isClient", true},
                     {"shardId", "another-shard"},
                     {"clientId", "43"}}})}};
  Check(stream.Update(update));
  Check(stream.entities.size() == 3 &&
        stream.entities.at("a").position[0] == 3);
  Check(stream.entities.at("a").isClient && !stream.entities.at("b").isClient &&
        stream.entities.at("c").isClient);
  Check(stream.entities.at("a").shardId == "new-shard" &&
        stream.entities.at("a").clientId == "42" &&
        stream.entities.at("b").clientId.empty() &&
        stream.entities.at("c").shardId == "another-shard" &&
        stream.entities.at("c").clientId == "43");
  Check(!stream.Update(update));
  update["revision"] = 1;
  update["sequence"] = 2;
  Check(!stream.Update(update));
  update["revision"] = 2;
  update["sequence"] = 3;
  Check(!stream.Update(update) && stream.inconsistent);
  stream.Snapshot({{"subscriptionId", "sub"},
                   {"revision", 3},
                   {"sequence", 0},
                   {"entities", JSON::array()}});
  Check(stream.entities.empty() && !stream.inconsistent);
  update["revision"] = 3;
  update["sequence"] = 1;
  update["deltas"] = JSON::array({{{"id", "a"}, {"type", "remove"}}});
  Check(stream.Update(update));
  Check(stream.id == "sub" && stream.revision == 3);
  stream.Clear();
  Check(stream.id.empty());
  std::cout << "Camera, clipping, layers, and entity stream checks passed\n";
}

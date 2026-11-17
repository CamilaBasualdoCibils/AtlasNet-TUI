#pragma once
#include "AtlasNet/ControlPanel/Protocol.hpp"
#include <array>
#include <map>
#include <string>
#include <vector>
namespace AtlasNetTUI
{
using JSON = AtlasNet::ControlPanel::JSON;
struct Point
{
  double x = 0, y = 0;
};
struct Bounds
{
  Point min, max;
  bool Contains(const Bounds&) const;
};
class Camera final
{
public:
  Point center{};
  double unitsPerPixel = 0.5;
  int width = 160, height = 80;
  Point WorldToScreen(Point) const;
  Point ScreenToWorld(Point) const;
  void Pan(Point delta);
  void Zoom(Point cursor, double factor);
  Bounds ViewBounds(double margin = 1) const;
};
struct Layers
{
  bool heuristic = true, entities = true, bounds = false;
};
struct Segment
{
  Point a, b;
};
// Exact convex region geometry, sliced at z=0 for XYZ partitions. Clipping
// edges introduced by the viewport are excluded from returned boundaries.
std::vector<Segment> RegionSegments(const JSON& region, const Bounds&,
                                    unsigned dimension = 2, double slice = 0);
bool RegionContains(const JSON& region, Point, unsigned dimension = 2,
                    double slice = 0);
struct MapEntity
{
  std::array<double, 3> position{};
  bool isClient = false;
  std::string shardId, clientId;
};
class EntityStream final
{
public:
  std::string id;
  uint64_t revision = 0, sequence = 0;
  bool inconsistent = false;
  std::map<std::string, MapEntity> entities;
  void Snapshot(const JSON&);
  bool Update(const JSON&);
  void Clear();
};
JSON SubscriptionParameters(const Bounds&, std::string plane);
Point Project(const std::array<double, 3>& position, std::string plane);
} // namespace AtlasNetTUI

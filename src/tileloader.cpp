/**
 * TileLoader class for managing:
 *    - GPS to tile coordinate conversions
 *    - Tile image caching
 *    - Downloading images
 *
 * Based on the work of Gareth Cross, from the rviz_satellite package
 *    https://github.com/gareth-cross/rviz_satellite
 *
 * Parker Lusk, 2018
 *
 ******************************************************************************
 * TileLoader.cpp
 *
 *  Copyright (c) 2014 Gaeth Cross. Apache 2 License.
 *
 *  This file is part of rviz_satellite.
 *
 *  Created on: 07/09/2014
 */

#include "gazebo_satellite/tileloader.h"

namespace fs = boost::filesystem;

static size_t replaceRegex(const boost::regex &ex, std::string &str,
                           const std::string &replace)
{
  std::string::const_iterator start = str.begin(), end = str.end();
  boost::match_results<std::string::const_iterator> what;
  boost::match_flag_type flags = boost::match_default;
  size_t count = 0;
  while (boost::regex_search(start, end, what, ex, flags)) {
    str.replace(what.position(), what.length(), replace);
    start = what[0].second;
    count++;
  }
  return count;
}

// ----------------------------------------------------------------------------

TileLoader::TileLoader(const std::string &service, double latitude,
                       double longitude, unsigned int zoom, unsigned int blocks)
    : latitude_(latitude), longitude_(longitude), zoom_(zoom),
      blocks_(blocks), object_uri_(service)
{
  assert(blocks_ >= 0);

  // const std::string package_path = ros::package::getPath("rviz_satellite");
  const std::string package_path = ".";
  if (package_path.empty()) {
    throw std::runtime_error("package 'rviz_satellite' not found");
  }

  std::hash<std::string> hash_fn;
  cache_path_ = fs::path(package_path + "/gzsatellite/mapscache/"
                          + std::to_string(hash_fn(object_uri_)));

  // Create the directory structure for the tile images
  fs::create_directories(cache_path_);

  /// @todo: some kind of error checking of the URL

  //  calculate center tile coordinates
  double x, y;
  latLonToTileCoords(latitude_, longitude_, zoom_, x, y);
  center_tile_x_ = std::floor(x);
  center_tile_y_ = std::floor(y);
  //  fractional component
  origin_offset_x_ = x - center_tile_x_;
  origin_offset_y_ = y - center_tile_y_;
}

// ----------------------------------------------------------------------------

bool TileLoader::insideCentreTile(double lat, double lon) const
{
  double x, y;
  latLonToTileCoords(lat, lon, zoom_, x, y);
  return (std::floor(x) == center_tile_x_ && std::floor(y) == center_tile_y_);
}

// ----------------------------------------------------------------------------

void TileLoader::start()
{
  // discard previous set of tiles and all pending requests
  abort();

  // gzdbg << "loading " << blocks_ << " blocks around tile (" << center_tile_x_ << ", " << center_tile_y_ << ")\n";

  // determine what range of tiles we can load
  const int min_x = std::max(0, center_tile_x_ - blocks_);
  const int min_y = std::max(0, center_tile_y_ - blocks_);
  const int max_x = std::min(maxTiles(), center_tile_x_ + blocks_);
  const int max_y = std::min(maxTiles(), center_tile_y_ + blocks_);

  // initiate blocking requests
  for (int y = min_y; y <= max_y; y++) {
    for (int x = min_x; x <= max_x; x++) {
      // Generate filename
      const fs::path full_path = cachedPathForTile(x, y, zoom_);

      // Check if tile is already in the cache
      if (fs::exists(full_path)) {
        tiles_.push_back(MapTile(x, y, zoom_, full_path.string()));

      } else {
        const std::string url = uriForTile(x, y);

        // send blocking request
        auto r = cpr::Get(url);

        // process the response
        if (r.status_code == 200) {
          // Save the response text (which is image data) as a binary
          std::fstream imgout(full_path.string(), std::ios::out | std::ios::binary);
          imgout.write(r.text.c_str(), r.text.size());
          imgout.close();

          // Let everyone know we have an image for this tile
          tiles_.push_back(MapTile(x, y, zoom_, full_path.string()));

        } else {
          gzerr << "Failed loading " << r.url << " with code " << r.status_code << std::endl;
        }
      }
    }
  }
}

// ----------------------------------------------------------------------------

double TileLoader::resolution() const
{
  return zoomToResolution(latitude_, zoom_);
}

// ----------------------------------------------------------------------------

/// @see http://wiki.openstreetmap.org/wiki/Slippy_map_tilenames
/// For explanation of these calculations.
void TileLoader::latLonToTileCoords(double lat, double lon, unsigned int zoom,
                                    double &x, double &y)
{
  if (zoom > 31) {
    throw std::invalid_argument("Zoom level " + std::to_string(zoom) + " too high");
  } else if (lat < -85.0511 || lat > 85.0511) {
    throw std::invalid_argument("Latitude " + std::to_string(lat) + " invalid");
  } else if (lon < -180 || lon > 180) {
    throw std::invalid_argument("Longitude " + std::to_string(lon) + " invalid");
  }

  const double rho = M_PI / 180;
  const double lat_rad = lat * rho;

  unsigned int n = (1 << zoom);
  x = n * ((lon + 180) / 360.0);
  y = n * (1 - (std::log(std::tan(lat_rad) + 1 / std::cos(lat_rad)) / M_PI)) / 2;

  // gzdbg << "Center tile coords: " << x << ", " << y << std::endl;
}

// ----------------------------------------------------------------------------

double TileLoader::zoomToResolution(double lat, unsigned int zoom)
{
  const double lat_rad = lat * M_PI / 180;
  return 156543.034 * std::cos(lat_rad) / (1 << zoom);
}

// ----------------------------------------------------------------------------

std::string TileLoader::uriForTile(int x, int y) const
{
  std::string object = object_uri_;
  //  place {x},{y},{z} with appropriate values
  replaceRegex(boost::regex("\\{x\\}", boost::regex::icase), object,
               std::to_string(x));
  replaceRegex(boost::regex("\\{y\\}", boost::regex::icase), object,
               std::to_string(y));
  replaceRegex(boost::regex("\\{z\\}", boost::regex::icase), object,
               std::to_string(zoom_));
  return object;
}

// ----------------------------------------------------------------------------

std::string TileLoader::cachedNameForTile(int x, int y, int z) const
{
  std::ostringstream os;
  os << "x" << x << "_y" << y << "_z" << z << ".jpg";
  return os.str();
}

// ----------------------------------------------------------------------------

fs::path TileLoader::cachedPathForTile(int x, int y, int z) const
{
  fs::path p = cache_path_ / cachedNameForTile(x, y, z);
  return p;
}

// ----------------------------------------------------------------------------

int TileLoader::maxTiles() const
{
  return (1 << zoom_) - 1;
}

// ----------------------------------------------------------------------------

void TileLoader::abort()
{
  tiles_.clear();
}
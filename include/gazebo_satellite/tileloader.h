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
 * TileLoader.h
 *
 *  Copyright (c) 2014 Gaeth Cross. Apache 2 License.
 *
 *  This file is part of rviz_satellite.
 *
 *  Created on: 07/09/2014
 */

#ifndef TILELOADER_H
#define TILELOADER_H

#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <memory>
#include <fstream>
#include <functional>
#include <stdexcept>

#include <gazebo/common/common.hh>
#include <gazebo/gazebo.hh>

#include <boost/filesystem.hpp>
#include <boost/regex.hpp>

#include <cpr/cpr.h>


class TileLoader {
public:
  class MapTile {
  public:     
    MapTile(int x, int y, int z, const std::string& path)
      : x_(x), y_(y), z_(z), path_(path) {}

    /// X tile coordinate.
    int x() const { return x_; }

    /// Y tile coordinate.
    int y() const { return y_; }
      
    /// Z tile zoom value.
    int z() const { return z_; }

    /// Image associated with this tile.
    const std::string& imagePath() const { return path_; }

  private:
    int x_;
    int y_;
    int z_;
    std::string path_;
  };

  explicit TileLoader(const std::string &service, double latitude,
                      double longitude, unsigned int zoom, unsigned int blocks);

  /// Start loading tiles asynchronously.
  void start();

  /// Meters/pixel of the tiles.
  double resolution() const;

  /// X index of central tile.
  int centerTileX() const { return center_tile_x_; }

  /// Y index of central tile.
  int centerTileY() const { return center_tile_y_; }

  /// Fraction of a tile to offset the origin (X).
  double originOffsetX() const { return origin_offset_x_; }

  /// Fraction of a tile to offset the origin (Y).
  double originOffsetY() const { return origin_offset_y_; }

  /// Test if (lat,lon) falls inside centre tile.
  bool insideCentreTile(double lat, double lon) const;

  /// Convert lat/lon to a tile index with mercator projection.
  static void latLonToTileCoords(double lat, double lon, unsigned int zoom,
                                 double &x, double &y);

  /// Convert latitude and zoom level to ground resolution.
  static double zoomToResolution(double lat, unsigned int zoom);

  /// Path to tiles on the server.
  const std::string &objectURI() const { return object_uri_; }

  /// Current set of tiles.
  const std::vector<MapTile> &tiles() const { return tiles_; }

  /// Cancel all current requests.
  void abort();

private:
  /// URI for tile [x,y]
  std::string uriForTile(int x, int y) const;

  /// Get name for cached tile [x,y,z]
  std::string cachedNameForTile(int x, int y, int z) const;

  /// Get file path for cached tile [x,y,z].
  boost::filesystem::path cachedPathForTile(int x, int y, int z) const;

  /// Maximum number of tiles for the zoom level
  int maxTiles() const;

  double latitude_;
  double longitude_;
  unsigned int zoom_;
  int blocks_;
  int center_tile_x_;
  int center_tile_y_;
  double origin_offset_x_;
  double origin_offset_y_;

  boost::filesystem::path cache_path_;

  std::string object_uri_;

  std::vector<MapTile> tiles_;
};

#endif // TILELOADER_H
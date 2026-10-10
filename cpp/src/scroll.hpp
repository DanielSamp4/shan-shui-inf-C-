#pragma once

#include "scenery.hpp"

#include <vector>

class Generator;

// Rasterize each piece on its own, then slide a view across them.
// The window stays open, and live keeps planning land ahead of the camera,
// until the window is closed. Returns true when the opening frames differ.
bool playScroll(const std::vector<Piece>& pieces, const std::string& seed = "1", double speed = 120.0,
                Generator* live = nullptr);

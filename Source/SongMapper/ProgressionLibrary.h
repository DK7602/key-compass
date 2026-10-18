// Song Mapper - built-in chord progression library (key-relative roman numerals)
#pragma once

#include <string>
#include <vector>

namespace sm
{
    struct Progression { std::string name; std::string roman; };
    struct Style       { std::string name; std::vector<Progression> progressions; };
    struct Genre       { std::string name; std::vector<Style> styles; };

    // Pop, Rock, Country, Rap / Hip-Hop, Classical, EDM, Cultural / World, Jazz, Funk, Worship
    const std::vector<Genre>& library();
}

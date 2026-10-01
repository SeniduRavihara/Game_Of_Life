#ifndef PATTERNS_H
#define PATTERNS_H

#include <string>
#include <vector>

struct PatternPoint {
    int dx;
    int dy;
};

struct Pattern {
    std::string name;
    int width;
    int height;
    std::vector<PatternPoint> points;
};

inline std::vector<Pattern> get_available_patterns() {
    std::vector<Pattern> list;

    // 1. Glider
    {
        Pattern p;
        p.name = "Glider";
        p.width = 3;
        p.height = 3;
        p.points = {
            {1, 0},
            {2, 1},
            {0, 2}, {1, 2}, {2, 2}
        };
        list.push_back(p);
    }

    // 2. Pulsar (period 3 oscillator)
    {
        Pattern p;
        p.name = "Pulsar";
        p.width = 13;
        p.height = 13;
        // Symmetric pulsar points centered around (6, 6)
        std::vector<int> coords = {2, 3, 4, 8, 9, 10};
        for (int x : coords) {
            p.points.push_back({x, 0});
            p.points.push_back({x, 5});
            p.points.push_back({x, 7});
            p.points.push_back({x, 12});
        }
        for (int y : coords) {
            p.points.push_back({0, y});
            p.points.push_back({5, y});
            p.points.push_back({7, y});
            p.points.push_back({12, y});
        }
        list.push_back(p);
    }

    // 3. Gosper Glider Gun (36 x 9)
    {
        Pattern p;
        p.name = "Gosper Gun";
        p.width = 36;
        p.height = 9;
        const char* gun_ascii[9] = {
            "........................O...........",
            "......................O.O...........",
            "............OO......OO............OO",
            "...........O...O....OO............OO",
            "OO........O.....O...OO..............",
            "OO........O...O.OO....O.O...........",
            "..........O.....O.......O...........",
            "...........O...O....................",
            "............OO......................"
        };
        for (int y = 0; y < 9; ++y) {
            for (int x = 0; x < 36; ++x) {
                if (gun_ascii[y][x] == 'O') {
                    p.points.push_back({x, y});
                }
            }
        }
        list.push_back(p);
    }

    // 4. Acorn (Methuselah)
    {
        Pattern p;
        p.name = "Acorn";
        p.width = 7;
        p.height = 3;
        p.points = {
            {1, 0},
            {3, 1},
            {0, 2}, {1, 2}, {4, 2}, {5, 2}, {6, 2}
        };
        list.push_back(p);
    }

    // 5. Pentadecathlon (period 15 oscillator)
    {
        Pattern p;
        p.name = "Pentadecathlon";
        p.width = 10;
        p.height = 3;
        for (int x = 0; x < 10; ++x) {
            if (x == 2 || x == 7) {
                p.points.push_back({x, 0});
                p.points.push_back({x, 2});
            } else {
                p.points.push_back({x, 1});
            }
        }
        list.push_back(p);
    }

    // 6. Diehard (vanishes after 130 generations)
    {
        Pattern p;
        p.name = "Diehard";
        p.width = 8;
        p.height = 3;
        p.points = {
            {6, 0},
            {0, 1}, {1, 1},
            {1, 2}, {5, 2}, {6, 2}, {7, 2}
        };
        list.push_back(p);
    }

    return list;
}

#endif // PATTERNS_H

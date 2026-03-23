// Test file for maze problem
#include <bits/stdc++.h>
using namespace std;

// ===== solution code (inlined for testing) =====
int H, W;
vector<string> grid;
int dist[505][505][2];
int dr[] = {-1, 1, 0, 0};
int dc[] = {0, 0, -1, 1};

int solve() {
    int sr, sc, er, ec;
    for (int i = 0; i < H; i++)
        for (int j = 0; j < W; j++) {
            if (grid[i][j] == 'S') { sr = i; sc = j; }
            if (grid[i][j] == 'E') { er = i; ec = j; }
        }

    memset(dist, -1, sizeof(dist));
    queue<tuple<int,int,int>> q;
    dist[sr][sc][0] = 0;
    q.push({sr, sc, 0});

    while (!q.empty()) {
        auto [r, c, key] = q.front(); q.pop();
        int d = dist[r][c][key];
        for (int i = 0; i < 4; i++) {
            int nr = r + dr[i], nc = c + dc[i];
            if (nr < 0 || nr >= H || nc < 0 || nc >= W) continue;
            char cell = grid[nr][nc];
            if (cell == 'W') continue;
            if (cell == 'D' && !key) continue;
            int nkey = key;
            if (cell == 'K') nkey = 1;
            if (dist[nr][nc][nkey] == -1) {
                dist[nr][nc][nkey] = d + 1;
                q.push({nr, nc, nkey});
            }
        }
    }

    int ans = -1;
    if (dist[er][ec][0] != -1) ans = dist[er][ec][0];
    if (dist[er][ec][1] != -1) {
        if (ans == -1) ans = dist[er][ec][1];
        else ans = min(ans, dist[er][ec][1]);
    }
    return ans;
}

int pass = 0, fail = 0;
void check(string name, int expected, int got) {
    if (expected == got) {
        printf("[PASS] %s: %d\n", name.c_str(), got);
        pass++;
    } else {
        printf("[FAIL] %s: expected %d, got %d\n", name.c_str(), expected, got);
        fail++;
    }
}

int main() {
    // Test 1: example 1 -> 20
    H = 4; W = 12;
    grid = {"WWWWWWWWWWWW","WE.W.S..W.KW","W..D..W....W","WWWWWWWWWWWW"};
    check("Example1", 20, solve());

    // Test 2: example 2 -> -1
    H = 6; W = 6;
    grid = {"WWWWWW","WEWS.W","W.WK.W","W.WD.W","W.W..W","WWWWWW"};
    check("Example2", -1, solve());

    // Test 3: direct path S->E no key/door needed
    H = 3; W = 5;
    grid = {"WWWWW","WSKE.W","WWWWW"};
    // Actually fix: SE adjacent
    H = 3; W = 4;
    grid = {"WWWW","WSEW","WWWW"};
    check("DirectSE", 1, solve());

    // Test 4: S and E separated, no door/key, simple path
    H = 3; W = 6;
    grid = {"WWWWWW","WS...EW","WWWWWW"};
    // fix border
    H = 3; W = 7;
    grid = {"WWWWWWW","WS....EW","WWWWWWW"};
    // This has W=7 but row length 8, fix
    H = 3; W = 8;
    grid = {"WWWWWWWW","WS.....EW","WWWWWWWW"};
    // row length should be W=8 not 9; fix
    H = 3; W = 9;
    grid = {"WWWWWWWWW","WS......EW","WWWWWWWWW"};
    // S at col1, E at col8, steps=7
    H = 3; W = 10;
    grid = {"WWWWWWWWWW","WS.......EW","WWWWWWWWWW"};
    // row length 11 vs W=10... just do it properly
    H = 3; W = 6;
    grid = {"WWWWWW","WS..EW","WWWWWW"};
    // col: W S . . E W -> E at col4, S at col1, dist=3
    check("SimplePath", 3, solve());

    printf("\n%d passed, %d failed\n", pass, fail);
    return 0;
}

#include <bits/stdc++.h>
using namespace std;

int H, W;
vector<string> grid;
int dist[505][505][2]; // dist[r][c][hasKey]
int dr[] = {-1, 1, 0, 0};
int dc[] = {0, 0, -1, 1};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    cin >> H >> W;
    grid.resize(H);
    for (int i = 0; i < H; i++) cin >> grid[i];

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
            if (cell == 'D' && !key) continue; // 没钥匙不能过门

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

    cout << ans << endl;
    return 0;
}

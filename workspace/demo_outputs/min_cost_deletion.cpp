#include <iostream>
#include <vector>
#include <algorithm>
#include <climits>
#include <queue>

using namespace std;

// 方法1：贪心算法 - 使用优先队列选择最小的代价
long long minCostGreedy(vector<int>& arr, int k) {
    int n = arr.size();
    if (k == 0) return 0;
    if (n < 2 * k) return LLONG_MAX; // 不可能删除k对
    
    // 使用优先队列（最小堆）存储所有可能的删除代价
    priority_queue<pair<int, int>, vector<pair<int, int>>, greater<pair<int, int>>> pq;
    
    // 初始化，将所有可能的删除对加入优先队列
    for (int i = 0; i < n - 1; i++) {
        pq.push({arr[i], i});
    }
    
    vector<bool> deleted(n, false);
    long long totalCost = 0;
    int pairsDeleted = 0;
    
    while (pairsDeleted < k && !pq.empty()) {
        auto [cost, idx] = pq.top();
        pq.pop();
        
        // 如果这个位置已经被删除或者相邻位置被删除，跳过
        if (deleted[idx] || deleted[idx + 1]) {
            continue;
        }
        
        // 删除这对元素
        totalCost += cost;
        deleted[idx] = true;
        deleted[idx + 1] = true;
        pairsDeleted++;
        
        // 更新相邻位置的状态
        // 如果idx-1存在且未被删除，现在它不能与idx-2组成对（因为idx-1右边是已删除的idx）
        // 如果idx+2存在且未被删除，现在它不能与idx+1组成对（因为idx+1左边是已删除的idx+1）
    }
    
    if (pairsDeleted < k) {
        return LLONG_MAX;
    }
    
    return totalCost;
}

// 方法2：动态规划
long long minCostDP(vector<int>& arr, int k) {
    int n = arr.size();
    if (k == 0) return 0;
    if (n < 2 * k) return LLONG_MAX;
    
    // dp[i][j] 表示前i个元素中删除j对的最小代价
    vector<vector<long long>> dp(n + 1, vector<long long>(k + 1, LLONG_MAX));
    
    // 初始化
    for (int i = 0; i <= n; i++) {
        dp[i][0] = 0;
    }
    
    for (int i = 2; i <= n; i++) {
        for (int j = 1; j <= k; j++) {
            // 选项1：不删除第i-1个元素（作为左边元素）
            dp[i][j] = dp[i-1][j];
            
            // 选项2：删除第i-2和第i-1个元素（作为一对）
            if (i >= 2 && j >= 1 && dp[i-2][j-1] != LLONG_MAX) {
                long long cost = dp[i-2][j-1] + arr[i-2];
                if (cost < dp[i][j]) {
                    dp[i][j] = cost;
                }
            }
        }
    }
    
    return dp[n][k];
}

// 方法3：优化的贪心算法 - 使用链表和优先队列
long long minCostOptimized(vector<int>& arr, int k) {
    int n = arr.size();
    if (k == 0) return 0;
    if (n < 2 * k) return LLONG_MAX;
    
    // 创建双向链表节点
    struct Node {
        int value;
        int index;
        Node* prev;
        Node* next;
        bool deleted;
    };
    
    vector<Node*> nodes(n);
    for (int i = 0; i < n; i++) {
        nodes[i] = new Node{arr[i], i, nullptr, nullptr, false};
    }
    
    // 连接链表
    for (int i = 0; i < n; i++) {
        if (i > 0) nodes[i]->prev = nodes[i-1];
        if (i < n-1) nodes[i]->next = nodes[i+1];
    }
    
    // 优先队列存储所有可能的删除对（代价，左节点索引）
    priority_queue<pair<int, int>, vector<pair<int, int>>, greater<pair<int, int>>> pq;
    
    // 初始化优先队列
    for (int i = 0; i < n - 1; i++) {
        pq.push({arr[i], i});
    }
    
    long long totalCost = 0;
    int pairsDeleted = 0;
    
    while (pairsDeleted < k && !pq.empty()) {
        auto [cost, idx] = pq.top();
        pq.pop();
        
        Node* left = nodes[idx];
        if (left->deleted || !left->next || left->next->deleted) {
            continue;
        }
        
        // 删除这对元素
        totalCost += cost;
        pairsDeleted++;
        
        Node* right = left->next;
        left->deleted = true;
        right->deleted = true;
        
        // 更新链表连接
        Node* prev = left->prev;
        Node* next = right->next;
        
        if (prev) prev->next = next;
        if (next) next->prev = prev;
        
        // 如果prev和next都存在且都未被删除，它们现在成为新的相邻对
        if (prev && next && !prev->deleted && !next->deleted) {
            pq.push({prev->value, prev->index});
        }
    }
    
    // 清理内存
    for (auto node : nodes) {
        delete node;
    }
    
    if (pairsDeleted < k) {
        return LLONG_MAX;
    }
    
    return totalCost;
}

int main() {
    // 测试用例
    vector<int> arr1 = {3, 1, 4, 2, 5};
    int k1 = 2;
    
    cout << "测试序列: ";
    for (int num : arr1) cout << num << " ";
    cout << endl;
    cout << "k = " << k1 << endl;
    
    long long result1 = minCostGreedy(arr1, k1);
    long long result2 = minCostDP(arr1, k1);
    long long result3 = minCostOptimized(arr1, k1);
    
    cout << "贪心算法结果: " << (result1 == LLONG_MAX ? "不可能" : to_string(result1)) << endl;
    cout << "动态规划结果: " << (result2 == LLONG_MAX ? "不可能" : to_string(result2)) << endl;
    cout << "优化贪心结果: " << (result3 == LLONG_MAX ? "不可能" : to_string(result3)) << endl;
    
    // 更多测试
    cout << "\n更多测试:" << endl;
    
    // 测试1: 简单情况
    vector<int> arr2 = {1, 2, 3, 4, 5};
    int k2 = 2;
    cout << "序列: 1 2 3 4 5, k=2" << endl;
    cout << "最小代价: " << minCostDP(arr2, k2) << " (应该为1+3=4)" << endl;
    
    // 测试2: 需要选择最小的k个代价
    vector<int> arr3 = {5, 1, 3, 2, 4};
    int k3 = 2;
    cout << "序列: 5 1 3 2 4, k=2" << endl;
    cout << "最小代价: " << minCostDP(arr3, k3) << " (应该为1+2=3)" << endl;
    
    // 测试3: 不可能的情况
    vector<int> arr4 = {1, 2};
    int k4 = 2;
    cout << "序列: 1 2, k=2" << endl;
    cout << "最小代价: " << (minCostDP(arr4, k4) == LLONG_MAX ? "不可能" : "可能") << endl;
    
    return 0;
}
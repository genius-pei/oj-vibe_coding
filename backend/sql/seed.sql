USE minioj;

INSERT INTO tags (name) VALUES
    ('数组'),
    ('字符串'),
    ('哈希表'),
    ('双指针'),
    ('排序'),
    ('二分查找'),
    ('栈'),
    ('队列'),
    ('链表'),
    ('树'),
    ('图'),
    ('动态规划'),
    ('贪心'),
    ('数学')
ON DUPLICATE KEY UPDATE name = VALUES(name);

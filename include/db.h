// 1. 检查数据库是否存在，如果不存在，报错或新建


// 2. 检查数据表是否存在，如果不存在，报错或新建
// 3. 插入配置项
// 4. 修改配置项
// 5. 删除配置项
// 6. 查询配置项


int sql_conn(const char *host, const char *user, const char *passwd, void *data);

int sql_query();

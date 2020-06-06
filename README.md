### LuaSQL3
A sqlite3 library specially developed for Lua 5.1 that brings good performance and easy connection to local databases.

### Documentation and Example
```Lua
local LuaSQL3 = require("lsql3");

-- LuaSQL3.connect(string) > Connect to a memory database or file database [return: database]
local database = LuaSQL3.connect(":memory:");
-- database:execute(string) > Execute any sql statement directly
database:execute([[
DROP TABLE IF EXISTS Friends;
CREATE TABLE Friends(Id INTEGER PRIMARY KEY, Name TEXT);
INSERT INTO Friends(Name) VALUES ('Tom');
INSERT INTO Friends(Name) VALUES ('Rebecca');
INSERT INTO Friends(Name) VALUES ('Jim');
INSERT INTO Friends(Name) VALUES ('Roger');
INSERT INTO Friends(Name) VALUES ('Robert');
]]);
-- database:prepare(string) > Prepare sql statement to fetch a result [return: cursor]
local cursor = db:prepare("SELECT * FROM Friends;");
-- cursor:fetchone() > Return one result, the first in the stack
local firstResult = cursor:fetchone();
-- The first index is the row, and the second index is the column id
print(firstResult[1][1]); -- > Id -> 1
print(firstResult[1][2]); -- > Name -> Tom
-- cursor:fetchall() > In the same statement collect all the remaining results
local remaining = cursor:fetchall();
-- cursor:close() > Finalize the statement
cursor:close();
-- Close database connection
database:close();
```

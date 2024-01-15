# Info
```
version 2.1.12
https://github.com/mehdi-sadighian/cpp-postgresql-Connection-Pool-And-Query-Suite

// FileCopyrightText: Mehdi Sadighian <https://msadighian.com>
// License-Identifier: GPL-3.0 license 
```

# postgresql-connection-pool-and-query-suite
postgresql connection pool  and query suite (c++)

# Dependencies:
```
libpq 
libpqxx
```
# build  example:

test OS : slackware 14.1 64bit,
test OS : ubuntu 18.04 TLS 64bit

## install dependencies on ubuntu :
```
sudo apt update 
sudo apt-get install build-essential -y
sudo apt-get install libpq-dev libpqxx-dev -y
```

# compile note:
```
compile with g++ and link with -lpq -lpqxx -lpthread
```

#  code example for including inside your project, mian.cpp:
```

#include <cstdio>
#include <unistd.h>
#include <string>
#include <iostream>

#include "PostgreSQL_ConnectionPool_V2/PostgreSQL_ConnectionPool_V2.h"

using namespace std;
using PGSQL_Settings = PostgresqlConnectionPool::PGSQL_Settings;
using PGSQL_ConnectionPool = PostgresqlConnectionPool::PGSQL_ConnectionPool;
using Select = PostgresqlConnectionPool::PGSQL_ConnectionPool::Select;

int main()
{
    PGSQL_Settings PGSettings;
    PGSettings.db_host = "127.0.0.1";
    PGSettings.db_port = "5432";
    PGSettings.db_name = "test";
    PGSettings.db_user = "root";
    PGSettings.db_password = "testpassword";
    PGSettings.max_connections = 20;
    PGSettings.GetColumnTypes = true;


    PGSQL_ConnectionPool PgsqlConnection(PGSettings);

    string Error;
    if (!PgsqlConnection.Connect(Error))
    {
        cout << Error << endl;
        cout << "Cannot Connect To Database...Exiting" << endl;
        exit(1);
    }

    //Create DataBase
    if (!PgsqlConnection.CreateDatabase("test11", Error))
        cout << Error << endl;
    else
        cout << "Database Creation Success" << endl;

    //custom query can be used for create table, insert, delete,update,.... BUT NOT SELECT
    //for select use select object

    string query;

    //create table example
    query = "create table t7 (id serial PRIMARY KEY NOT NULL UNIQUE,familyname varchar(256) );";
    if (!PgsqlConnection.CustomQuery(query, Error))
        cout << Error << endl;
    else
        cout << "CustomQuery Success" << endl;


    //insert example
    query = "insert into t7 (familyname) VALUES (\'mehdi.sadighian@hotmail.com\');";
    if (!PgsqlConnection.CustomQuery(query, Error))
        cout << Error << endl;
    else
        cout << "CustomQuery Success" << endl;

    //insert retunring id example:
    query = "insert into t7 (familyname) VALUES (\'mehdi.sadighian@hotmail.com\') RETURNING id;";
    string id = PgsqlConnection.InsertReturnID(query, Error);

    if (id == "-1")
        cout << Error << endl;
    else
        cout << "insert retunring id Success, id=" << id << endl;


    //Select Example:

    query = "select id,familyname from t7;";

    //Creare Object, This Will Dispose And Clear itself when goes out of scope
    Select PGSelect(&PgsqlConnection, query);

    if (PGSelect.Success)
    {
        cout << "####\nprint select result example 1:" << endl;
        for (int i = 0; i < PGSelect.GetNumberOfSelectedRows(); i++)
        {
            Select::Row& row = PGSelect.GetRow(i);
            string& column1 = row[0];
            string& column2 = row[1];
            cout << column1 << endl;
            cout << column2 << endl;
        }

        cout << "####\nprint select result example 2:" << endl;

        for (int i = 0; i < PGSelect.GetNumberOfSelectedRows(); i++)
        {
            for (auto& column : PGSelect.GetRow(i))
            {
                cout << "column value=" << column << endl;
            }
        }

        cout << "####\nprint only selected column DataType:" << endl;
        for (int i = 0; i < PGSelect.SelectedColumnsInfo.size(); i++)
        {
            auto& node = PGSelect.SelectedColumnsInfo[i];
            cout << node->name << endl;
            cout << node->type << endl;
            cout << node->typeName << endl;
        }

        cout << "####\nprint select result and column DataTypes example:" << endl;

        for (int i = 0; i < PGSelect.GetNumberOfSelectedRows(); i++)
        {
            Select::Row& row = PGSelect.GetRow(i);

            auto& node1 = PGSelect.SelectedColumnsInfo[0];
            string& column1 = row[0];

            auto& node2 = PGSelect.SelectedColumnsInfo[1];
            string& column2 = row[1];

            cout << "####" << endl;
            cout << "column1 name:" << node1->name << endl;
            cout << "column1 value:" << column1 << endl;
            cout << "column1 type:" << node1->type << endl;
            cout << "column1 typeName:" << node1->typeName << endl;

            cout << "####" << endl;
            cout << "column2 name:" << node2->name << endl;
            cout << "column2 value:" << column2 << endl;
            cout << "column2 type:" << node2->type << endl;
            cout << "column2 typeName:" << node2->typeName << endl;

        }
       
    }

}//main
```

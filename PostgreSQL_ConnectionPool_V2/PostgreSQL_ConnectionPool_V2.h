#pragma once

#ifndef POSTGRESQL_CONNECTION_POOL_V2
#define POSTGRESQL_CONNECTION_POOL_V2 1

#define VERSION 2.1.12

#include <cstdio>
#include <unistd.h>
#include <string>
#include <iostream>
#include <pqxx/pqxx>
#include <condition_variable>
#include <queue>
#include <mutex>
#include <map>

using namespace std;
using namespace pqxx;


namespace PostgresqlConnectionPool
{
	class PGSQL_Settings
	{
	public:
		string db_name;
		string db_user;
		string db_password;
		string db_host;
		string db_port = "5432";

		//max connections
		int max_connections = 20;

		//column types
		bool GetColumnTypes = false;
	};


	class PGSQL_ConnectionPool
	{
	public:
		
		explicit PGSQL_ConnectionPool(const PGSQL_Settings& Settings) :
			Settings(Settings)
		{
			
		}//PGSQL_ConnectionPool
		
		~PGSQL_ConnectionPool()
		{
		}//~PGSQL_ConnectionPool

		bool Connect(string& Error);
		bool CreateDatabase(string dbname, string& Error);
		bool CustomQuery(string& query, string& Error);
		string InsertReturnID(string& query, string& Error);

	private:
		string ConnectionString;
		const PGSQL_Settings& Settings;

		//queue to store pgsql connections
		queue<std::shared_ptr<pqxx::lazyconnection>> conList;
		mutex conList_mutex;

		//condition variable to notify threads waiting that we have one connection free
		condition_variable pgsql_pool_condition;

		map <int, string> TypeNames;

	protected:

		bool SetupConnectionPool(string& Error);
		shared_ptr<pqxx::lazyconnection> ConnectionPickup();
		void ConnectionHandBack(std::shared_ptr<pqxx::lazyconnection> conn_);
		bool GetTypeOid();

	public:
		class Select
		{
			friend class PGSQL_ConnectionPool;

		public:
			bool Success = false;
			string Error;

			struct ColumnInfo
			{
				string name;
				int type;
				string typeName;
			};

			typedef vector<string> Row;
			vector<ColumnInfo*> SelectedColumnsInfo;

		public:

			explicit Select(PGSQL_ConnectionPool* Parent, string& query) :
				Parent(Parent), query(query)
			{
				Success = SelectFromDB();
				if (Success && Parent->Settings.GetColumnTypes)
					ConvertSelectResultTypes();
			}

			~Select() { this->ClearResult(); }

			void ReSelect(string& query)
			{
				this->Clear();
				this->NumberOfSelectedRows = 0;
				this->NumberOfSelectedColumns = 0;
				this->Success = false;

				Success = SelectFromDB();
				if (Success && Parent->Settings.GetColumnTypes)
					ConvertSelectResultTypes();
			}

			void Clear() { this->ClearResult(); }

			inline int GetNumberOfSelectedRows() const { return this->NumberOfSelectedRows; }
			inline int GetNumberOfSelectedColumns() const { return this->NumberOfSelectedColumns; }

			inline Row& GetRow(int index) {  
				if (index < NumberOfSelectedRows) 
					return *SelectResult[index]; 
				else
				{
					Error = "Error, Request Index Is Out Of Range";
					cout << Error << endl;
				}
			};

		private:
			PGSQL_ConnectionPool* Parent;
			vector<Row*> SelectResult;
			
			string& query;
			bool SetColumnTypes;
			int NumberOfSelectedRows = 0;
			int NumberOfSelectedColumns = 0;

		private:
			bool SelectFromDB();
			void ClearResult();
			void ConvertSelectResultTypes();

		};//class Select
	};//class PGSQL_ConnectionPool
	

}//namespace PostgresqlConnectionPoolV2

#endif // !POSTGRESQL_CONNECTION_POOL_V2 1


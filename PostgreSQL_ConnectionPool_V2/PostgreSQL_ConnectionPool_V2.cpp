#include "PostgreSQL_ConnectionPool_V2.h"

namespace PostgresqlConnectionPool
{
	bool PGSQL_ConnectionPool::Connect(string& Error)
	{
		this->ConnectionString = "dbname= " + Settings.db_name;
		this->ConnectionString += " user=" + Settings.db_user;
		this->ConnectionString += " password=" + Settings.db_password;
		this->ConnectionString += " hostaddr=" + Settings.db_host;
		this->ConnectionString += " port=" + Settings.db_port;
		this->ConnectionString += " connect_timeout=6";

		return this->SetupConnectionPool(Error);
	}//connect


	bool PGSQL_ConnectionPool::SetupConnectionPool(string& Error)
	{
		//check if wen can connect to database
		try
		{
			connection C(ConnectionString);

			if (C.is_open())
			{
				//close database connection
				C.disconnect();
			}
			else {
				Error = "Can't open database";
				return false;
			}//if
		}
		catch (const exception& e)
		{
			Error = string(e.what());
			return false;
		}//catch

		for (int i = 0; i < Settings.max_connections; i++)
		{
			//conList.emplace(std::make_shared<pqxx::lazyconnection>());
			try
			{
				conList.emplace(std::make_shared<pqxx::lazyconnection>(this->ConnectionString));
			}
			catch (const exception& e)
			{
				Error = string(e.what());
				return false;
			}

		}//for

		if (Settings.GetColumnTypes)
		{
			if (!GetTypeOid())
			{
				Error = "Cannot Get Type Oid";
				return false;
			}
		}

		return true;

	}//SetupConnectionPool

	bool PGSQL_ConnectionPool::GetTypeOid()
	{
		string query = "select oid, typname from pg_type;";
		PostgresqlConnectionPool::PGSQL_ConnectionPool::Select Select(this, query);

		if (!Select.Success)
			return false;

		for (auto& row : Select.SelectResult)
		{
			try
			{
				TypeNames.emplace(stoi((*row)[0]), (*row)[1]);
			}
			catch (exception& e)
			{
				cout << "GetTypeOid Exception:" << e.what() << endl;
				return false;
			}
		}

		return true;
	}//GetTypeOid

	shared_ptr<pqxx::lazyconnection> PGSQL_ConnectionPool::ConnectionPickup()
	{
		std::unique_lock<std::mutex> lock_(conList_mutex);

		//if pool is empty, we have no connection available, so we wait until a connection become available
		while (conList.empty()) {

			pgsql_pool_condition.wait(lock_);//wait for notfiy

		}//while

		//when program reachs this point, we have an available connection in pool

		//get first element (connection) in the queue
		auto conn_ = conList.front();

		//remove first element from queue
		conList.pop();
		return conn_;

	}//ConnectionPickup

	void PGSQL_ConnectionPool::ConnectionHandBack(std::shared_ptr<pqxx::lazyconnection> conn_)
	{
		std::unique_lock<std::mutex> lock_(conList_mutex);

		//hand back connection into pool
		conList.push(conn_);

		// unlock mutex
		lock_.unlock();

		// notify one of thread that is waiting
		pgsql_pool_condition.notify_one();

		return;
	}//ConnectionHandBack

	bool PGSQL_ConnectionPool::CreateDatabase(string dbname,string& Error)
	{
		const char* sql;
		
		string query = "CREATE DATABASE " + dbname + ";";
		sql = query.c_str();

		auto conn = ConnectionPickup();

		//if we have nullptr in conn
		if (!conn)
		{
			Error = "Can't Get a Connection To Database";
			return false;
		}

		try
		{
			// create a nontransaction from a connection
			pqxx::nontransaction W(reinterpret_cast<pqxx::lazyconnection&>(*conn.get()));
				
			try {
				/* Execute SQL query */
				W.exec(sql);
				W.commit();
			}//try
			catch (const exception& e) {
				Error = "CREATE DATABASE:" + string(e.what());
				Error += "Query Was:" + query;
				
				W.abort();
				ConnectionHandBack(conn);
				return false;
			}//catch

		}//try
		catch (const exception& e) {
			Error = "Work Create:" + string(e.what());
			ConnectionHandBack(conn);
			return false;
		}//catch
	
		ConnectionHandBack(conn);
		return true;

	}//CreateDatabase


	bool PGSQL_ConnectionPool::CustomQuery(string& query,string& Error)
	{
		const char* sql;
		bool success = false;
		sql = query.c_str();

		auto conn = ConnectionPickup();

		//if we have nullptr in conn
		if (!conn)
		{
			Error = "Can't Get a Connection To Database";
			return false;
		}

		try
		{
			// create a transaction from a connection
			work W(reinterpret_cast<pqxx::lazyconnection&>(*conn.get()));

			try {

				/* Execute SQL query */
				W.exec(sql);
				W.commit();
			}//try
			catch (const std::exception& e) {
				Error =  "Custom Query ERROR:" + string(e.what());
				W.abort();
				ConnectionHandBack(conn);
				return false;
			}//catch

		}//try
		catch (const exception& e) {
			Error = "Work Create:" + string(e.what());
			ConnectionHandBack(conn);
			return false;
		}//catch

		//hand back pgsql_connection
		ConnectionHandBack(conn);

		return true;

	}//CustomQuery

	string PGSQL_ConnectionPool::InsertReturnID(string& query, string& Error)
	{
		string id = "-1";

		const char* sql;
		
		sql = query.c_str();

		auto conn = ConnectionPickup();

		//if we have nullptr in conn
		if (!conn)
		{
			Error = "Can't Get a Connection To Database";
			return id;
		}

		try
		{
			// create a transaction from a connection
			work W(reinterpret_cast<pqxx::lazyconnection&>(*conn.get()));

			try {

				/* Execute SQL query */
				result R(W.exec(sql));
				result::const_iterator c = R.begin();
				
				if (c[0].is_null())
				{
					ConnectionHandBack(conn);
					return id;
				}
				else
				{
					id = c[0].as<string>();
					W.commit();
				}
			}//try
			catch (const std::exception& e) {
				Error = "InsertReturnID ERROR:" + string(e.what());
				W.abort();
				ConnectionHandBack(conn);
				return "-1";
			}//catch

		}//try
		catch (const std::exception& e) {
			Error = "Work Create:" + string(e.what());
			ConnectionHandBack(conn);
			return "-1";
		}//catch

		//hand back pgsql_connection
		ConnectionHandBack(conn);

		return id;
	}//InsertReturnID



	//select
	
	void PGSQL_ConnectionPool::Select::ClearResult()
	{
		if (!SelectResult.empty())
		{
			for (int i = 0; i < SelectResult.size(); i++)
				delete (SelectResult[i]);

			SelectResult.clear();
		}
		

		if (!SelectedColumnsInfo.empty())
		{
			for (int i = 0; i < SelectedColumnsInfo.size(); i++)
				delete (SelectedColumnsInfo[i]);

			SelectedColumnsInfo.clear();
		}
			

	}//ClearResult

	void PGSQL_ConnectionPool::Select::ConvertSelectResultTypes()
	{
		map<int, string>::iterator IT;

		for (int i = 0; i < SelectedColumnsInfo.size(); i++)
		{
			auto& node = SelectedColumnsInfo[i];

			IT = Parent->TypeNames.find(node->type);

			if (IT != Parent->TypeNames.end())
				node->typeName = IT->second;
		}

	}//ConvertSelectResultTypes

	bool PGSQL_ConnectionPool::Select::SelectFromDB()
	{
		ClearResult();

		const char* sql;
		sql = query.c_str();

		auto conn = Parent->ConnectionPickup();

		//if we have nullptr in conn
		if (!conn)
		{
			Error = "Can't Get a Connection To Database";
			return false;
		}

		try {

			/* Create a non-transactional object. */
			work N(reinterpret_cast<pqxx::lazyconnection&>(*conn.get()));

			/* Execute SQL query */
			result R(N.exec(sql));

			NumberOfSelectedRows = R.affected_rows();
			
			if (NumberOfSelectedRows <= 0)
			{
				N.abort();
				R.clear();
				Parent->ConnectionHandBack(conn);
				return true;
			}

			SelectResult.resize(NumberOfSelectedRows);

			int j = 0;

			for (result::const_iterator c = R.begin(); c != R.end(); ++c)
			{
				Row* RW = new Row(c.size());

				for (int i = 0; i < c.size(); i++)
				{
					if (!c[i].is_null())
					{
						(*RW)[i] = move(c[i].as<string>());
						
					/*	string temp = c[i].as<string>();
						std::cout << R.column_name(i) << '\n';
						std::cout << R.column_type(i) << '\n';
						cout << temp << endl;*/

					}
				}//for

				SelectResult[j] = RW;
				j++;
			}

			NumberOfSelectedColumns = R.columns();

			SelectedColumnsInfo.resize(NumberOfSelectedColumns);

			for (int col = 0; col < NumberOfSelectedColumns; ++col)
			{
				ColumnInfo* CI = new ColumnInfo();
				CI->name = R.column_name(col);
				CI->type = R.column_type(col);

				SelectedColumnsInfo[col] = CI;
				/*std::cout << R.column_name(col) << '\n';
				std::cout << R.column_type(col) << '\n';*/
			}

			N.abort();
			R.clear();
		}
		catch (const std::exception& e) {
			Error = "Select Error :" + string(e.what());
			Parent->ConnectionHandBack(conn);
			return false;
		}
		
		Parent->ConnectionHandBack(conn);
		return true;

	}//SelectFromDB


}//namespace PostgresqlConnectionPool

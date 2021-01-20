// https://zetcode.com/db/mysqlc/
#include <mysql.h>
#include <stdio.h>
#include <stdlib.h>

void finish_with_error(MYSQL *con)
{
	fprintf(stderr, "%s\n", mysql_error(con));
	mysql_close(con);
	exit(1);
}

int main(int argc, char **argv)
{
	MYSQL *con = mysql_init(NULL);

	if (con == NULL) {
		fprintf(stderr, "%s\n", mysql_error(con));
		exit(1);
	}

	if (mysql_real_connect(con, "localhost", "root", "MobilisCan", "neo", 0, NULL, 0) == NULL)
		finish_with_error(con);

	if (mysql_query(con, "DROP TABLE IF EXISTS cars"))
		finish_with_error(con);

	if (mysql_query(con, "CREATE TABLE cars(id INT PRIMARY KEY AUTO_INCREMENT, name VARCHAR(255), price INT)"))
		finish_with_error(con);

	if (mysql_query(con, "INSERT INTO cars VALUES(1,'Audi',52642)"))
		finish_with_error(con);

	mysql_close(con);
	exit(0);
}

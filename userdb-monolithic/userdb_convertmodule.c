/*
 * UserDB
 */

#include "./userdb.h"

void userdb_convertdatabase()
{
unsigned int count, errorcount;
char buf[1000];
char username[500];
char email[500];
char *temp;
FILE *database;
userdb_user *user;

	database = fopen("users.enc", "r");
	
	if ( !database )
		return;
	
	count = 0;
	errorcount = 0;
	
	while ( !feof(database) )
	{
		fgets(buf, 900, database);
		strcpy(username, buf);
		fgets(buf, 900, database);
		fgets(buf, 900, database);
		fgets(buf, 900, database);
		fgets(buf, 900, database);
		fgets(buf, 900, database);
		fgets(buf, 900, database);
		fgets(buf, 900, database);
		fgets(buf, 900, database);
		fgets(buf, 900, database);
		strcpy(email, buf);
		
    	while ( ircd_strcmp("end\n", buf) )
		    fgets(buf, 900, database);
		
		if ( feof(database) )
			break;
		
		temp = username;
		
		while ( *temp != '\n' && *temp != '\r' && *temp != '\t' )
		    temp++;
		
		*temp = '\0';
		
		temp = email;
		
		while ( *temp != '\n' && *temp != '\r' && *temp != '\t' )
		    temp++;
		
		*temp = '\0';
		
		if ( userdb_createuser(username, userdb_generatepassword(), email, &user) != USERDB_OK )
		{
			Error("userdb", ERR_INFO, "Error adding: %s (%s)", username, email);
			errorcount++;
		}
		
		if ( errorcount > 5 )
		{
		    Error("userdb", ERR_INFO, "Too many errors, aborting convert.");
		    break;
		}
		
		count++;
		
		/*
		if ( count > 50 )
		{
		    Error("userdb", ERR_INFO, "Maximum conversion limit reached");
		    break;
		}
		*/
		    
		if ( count % 1000 == 0 )
		    Error("userdb", ERR_INFO, "Current user: %u", count);
	}
	
	fclose(database);
}

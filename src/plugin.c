#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <mosquitto.h>
#include <mosquitto_plugin.h>
#include <fnmatch.h>
#include "mysql.h"
#include "bcrypt.h"

struct userdata {
        MYSQL *mysql;
        char *host;
        char *user;
        char *passwd;
        char *db;
};



int mosquitto_auth_plugin_version(void)
{
  return MOSQ_AUTH_PLUGIN_VERSION;
}

int mosquitto_auth_plugin_init(void **userdata, struct mosquitto_auth_opt *auth_opts, int auth_opt_count)
{
  int i;
  struct mosquitto_auth_opt *o;
  struct userdata *ud;
  int ret = MOSQ_ERR_SUCCESS;

  *userdata = (struct userdata *)malloc(sizeof(struct userdata));
  if (*userdata == NULL) {
    perror("allocting userdata");
    return MOSQ_ERR_UNKNOWN;
  }

  ud = *userdata;
  ud->mysql  = NULL;
  ud->host= NULL;
  ud->user = NULL;
  ud->passwd   = NULL;
  ud->db   = NULL;

  for (i = 0, o = auth_opts; i < auth_opt_count; i++, o++) {
#ifdef DEBUG
    fprintf(stderr, "AuthOptions: key=%s, val=%s\n", o->key, o->value);
#endif
    if (!strcmp(o->key, "mysql_user"))
      ud->user = strdup(o->value);
    if (!strcmp(o->key, "mysql_host"))
      ud->host = strdup(o->value);
    if (!strcmp(o->key, "mysql_passwd"))
      ud->passwd = strdup(o->value);
    if (!strcmp(o->key, "mysql_db"))
      ud->db = strdup(o->value);
  }

  if (ud->host == NULL)
    ud->host = strdup("localhost");

  ud->mysql = db_init(ud->host, ud->user, ud->passwd, ud->db);
  if (ud->mysql == NULL) {
    fprintf(stderr, "Cannot connect to mysql on %s\n", ud->host);
    ret = MOSQ_ERR_UNKNOWN;
  }else{
    fprintf(stderr, "Connected to mysql on %s\n", ud->host);
  
  }

  return (ret);
}

int mosquitto_auth_plugin_cleanup(void *userdata, struct mosquitto_auth_opt *auth_opts, int auth_opt_count)
{
  struct userdata *ud = (struct userdata *)userdata;

  if (ud) {
    if (ud->mysql) {
      db_destroy(ud->mysql);
    }
    if (ud->user)
      free(ud->user);
    if (ud->host)
      free(ud->host);
    if (ud->passwd)
      free(ud->passwd);
    if (ud->db)
      free(ud->db);
  }

  return MOSQ_ERR_SUCCESS;
}

int mosquitto_auth_security_init(void *userdata, struct mosquitto_auth_opt *auth_opts, int auth_opt_count, bool reload)
{
  return MOSQ_ERR_SUCCESS;
}

int mosquitto_auth_security_cleanup(void *userdata, struct mosquitto_auth_opt *auth_opts, int auth_opt_count, bool reload)
{
  return MOSQ_ERR_SUCCESS;
}

int mosquitto_auth_acl_check(void *userdata, const char *clientid, const char *username, const char *topic, int access)
{

 printf("HI\n");
#ifdef DEBUG
	fprintf(stderr, "acl_check u=%s, t=%s, a=%d\n",
		(username) ? username : "NIL",
		(topic) ? topic : "NIL",
		access);

#endif

 printf("RETURNING\n");

	return MOSQ_ERR_SUCCESS;
}
int IGNORE_mosquitto_auth_acl_check(void *userdata, const char *clientid, const char *username, const char *topic, int access)
{
   struct userdata *ud = (struct userdata*) userdata;
   printf("HI");
#ifdef DEBUG
	fprintf(stderr, "acl_check u=%s, t=%s, a=%d\n",
		(username) ? username : "NIL",
		(topic) ? topic : "NIL",
		access);
#endif

  if (!username || !*username)
		return MOSQ_ERR_ACL_DENIED;
  /* FIXME: what to do if not connected to db? */
  if (ud->mysql == NULL){
	fprintf(stderr, "acl_check u=%s, t=%s DENIED because no conn to db.\n", username, topic); 
	return MOSQ_ERR_ACL_DENIED;
  }
 long * result;
 int io = 0;
 if ((result = db_can_user_access_topic(ud->mysql, username, topic, &io)) == NULL) {
#ifdef DEBUG
    fprintf(stderr, "User %s can't access %s\n", username, topic);
#endif
    return MOSQ_ERR_ACL_DENIED;
  }
  free(result);
  //return MOSQ_ERR_SUCCESS;
  return MOSQ_ERR_ACL_DENIED;
}


int mosquitto_auth_unpwd_check(void *userdata, const char *username, const char *password)
{
  struct userdata *ud = (struct userdata *)userdata;
  char *phash;
  int match, io;
  io = 0;

#ifdef DEBUG
  fprintf(stderr, "auth_unpwd_check u=%s, p=%s\n",
    (username) ? username : "NIL",
    (password) ? password : "NIL");
#endif


  if (!username || !*username || !password || !*password)
    return MOSQ_ERR_AUTH;

  if (ud->mysql == NULL)
    return MOSQ_ERR_AUTH;

  if ((phash = db_getuser_passwd(ud->mysql, username, &io)) == NULL) {
#ifdef DEBUG
    fprintf(stderr, "User %s not found\n", 
      username);
#endif
    return MOSQ_ERR_AUTH;
  }

  if (io != 0) {
    fprintf(stderr, "Mysql IO error. Attempt to reconnect...\n");
    db_destroy(ud->mysql);

    ud->mysql = db_init(ud->host, ud->user, ud->passwd, ud->db);
    if (ud->mysql == NULL) {
      fprintf(stderr, "Cannot connect to mysql on %s\n", ud->host);
    }

    /* FIXME: reconnected? now what? Currently, just return failed for this time. */
    return MOSQ_ERR_AUTH;
  }

  char outhash[60];
  bcrypt_hashpw(password, phash, outhash);
 
  match = strcmp(phash, outhash) ;

#ifdef DEBUG
  fprintf(stderr, "unpwd_check: for user=%s, got: %s\n", username, phash);
  fprintf(stderr, "unpwd_check:  match == %d\n", match);
#endif

  free(phash);
  return (match == 0) ? MOSQ_ERR_SUCCESS : MOSQ_ERR_AUTH;
}

int mosquitto_auth_psk_key_get(void *userdata, const char *hint, const char *identity, char *key, int max_key_len)
{
  return MOSQ_ERR_AUTH;
}


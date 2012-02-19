MongoDB module for FreeRADIUS
=============================

This module allow you to use MongoDB as FreeRADIUS backend (instead of a LDAP).

How it works ?
--------------

Each time your radius receives a authorization request, FreeRADIUS will check user credentials stored in MongoDB.

The request looks like :

	{
		"mac": "00-11-22-33-44-55",
		"username": "john"
	}


If a document matches, MongoDB will return :

	{
		.. some data ..
		"password": "secret"
		.. some data ..
	}


FreeRADIUS will now compare given password and MongoDB password (only cleartext password).

Install
-------

* Copy the directory rlm_mongo/ into src/modules/
* Add "rlm_mongo" in src/modules/stable
* Build as usual
	* ./configure
	* make
	* make install
* Edit your configuration
	* Create a file named mongo in raddb/modules/ and insert your configuration (see below)
	* Add in your site configuration in authorize sub-section "mongo"
* Run radiusd

Configuration
-------------

	mongo {
		port = "27017"
		ip = "192.168.1.181"

		base = 	"production.users"
		username_field = "username"
		password_field = "password"

		# Check mac address (optionnal)
		# mac_field = "mac"

		# Check enable account (optionnal)
		# enable_field = "activate"
	}


Tips
----

If you use rlm_mongo in inner-tunnel and mac filter, ensure you have this in eap.conf : `copy_request_to_tunnel = yes`

If you want to use NTLM password, only replace `Cleartext-Password` by `NT-Password` in rlm_mongo.c

Credits
-------

Guillaume Rose, Roman Shterenzon

// Copyright (C) 2012-2013 Internet Systems Consortium, Inc. ("ISC")
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
// REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
// AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
// INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
// LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
// OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
// PERFORMANCE OF THIS SOFTWARE.

#ifndef SCHEMA_COPY_H
#define SCHEMA_COPY_H

namespace {

// What follows is a set of statements that creates a copy of the schema
// in the test database.  It is used by the MySQL unit test prior to each
// test.
//
// Each SQL statement is a single string.  The statements are not terminated
// by semicolons, and the strings must end with a comma.  The final line
// statement must be NULL (not in quotes)

// NOTE: This file mirrors the schema in src/lib/dhcpsrv/dhcpdb_create.mysql.
//       If this file is altered, please ensure that any change is compatible
//       with the schema in dhcpdb_create.mysql.

// Deletion of existing tables.

const char* destroy_statement[] = {
    "DROP TABLE lease4",
    "DROP TABLE lease6",
    "DROP TABLE lease6_types",
    "DROP TABLE schema_version",
    NULL
};

// Creation of the new tables.

const char* create_statement[] = {
    "START TRANSACTION",
    "CREATE TABLE lease4 ("
        "address INT UNSIGNED PRIMARY KEY NOT NULL,"
        "hwaddr VARBINARY(20),"
        "client_id VARBINARY(128),"
        "valid_lifetime INT UNSIGNED,"
        "expire TIMESTAMP,"
        "subnet_id INT UNSIGNED,"
        "fqdn_fwd BOOL,"
        "fqdn_rev BOOL,"
        "hostname VARCHAR(255)"
        ") ENGINE = INNODB",

    "CREATE INDEX lease4_by_hwaddr_subnet_id ON lease4 (hwaddr, subnet_id)",

    "CREATE INDEX lease4_by_client_id_subnet_id ON lease4 (client_id, subnet_id)",

    "CREATE TABLE lease6 ("
        "address VARCHAR(39) PRIMARY KEY NOT NULL,"
        "duid VARBINARY(128),"
        "valid_lifetime INT UNSIGNED,"
        "expire TIMESTAMP,"
        "subnet_id INT UNSIGNED,"
        "pref_lifetime INT UNSIGNED,"
        "lease_type TINYINT,"
        "iaid INT UNSIGNED,"
        "prefix_len TINYINT UNSIGNED,"
        "fqdn_fwd BOOL,"
        "fqdn_rev BOOL,"
        "hostname VARCHAR(255)"
        ") ENGINE = INNODB",

    "CREATE INDEX lease6_by_iaid_subnet_id_duid ON lease6 (iaid, subnet_id, duid)",

    "CREATE TABLE lease6_types ("
        "lease_type TINYINT PRIMARY KEY NOT NULL,"
        "name VARCHAR(5)"
        ")",

    "INSERT INTO lease6_types VALUES (0, \"IA_NA\")",
    "INSERT INTO lease6_types VALUES (1, \"IA_TA\")",
    "INSERT INTO lease6_types VALUES (2, \"IA_PD\")",

    "CREATE TABLE schema_version ("
        "version INT PRIMARY KEY NOT NULL,"
        "minor INT"
        ")",

    "INSERT INTO schema_version VALUES (1, 0)",
    "COMMIT",

    NULL
};

};  // Anonymous namespace

#endif // SCHEMA_COPY_H

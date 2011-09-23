// Copyright (C) 2011  Internet Systems Consortium, Inc. ("ISC")
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

#include <boost/scoped_ptr.hpp>

#include <datasrc/factory.h>
#include <datasrc/data_source.h>
#include <datasrc/sqlite3_accessor.h>

#include <dns/rrclass.h>
#include <cc/data.h>

#include <gtest/gtest.h>

using namespace isc::datasrc;
using namespace isc::data;

namespace {

// The default implementation is NotImplemented
TEST(FactoryTest, memoryClient) {
    DataSourceClientContainer client("memory", ElementPtr());
}

TEST(FactoryTest, badType) {
    ASSERT_THROW(DataSourceClientContainer("foo", ElementPtr()), DataSourceError);
}

TEST(FactoryTest, sqlite3ClientBadConfig) {
    ElementPtr config;
    ASSERT_THROW(DataSourceClientContainer("sqlite3", config),
                 DataSourceConfigError);

    config = Element::create("asdf");
    ASSERT_THROW(DataSourceClientContainer("sqlite3", config),
                 DataSourceConfigError);

    config = Element::createMap();
    ASSERT_THROW(DataSourceClientContainer("sqlite3", config),
                 DataSourceConfigError);

    config->set("class", ElementPtr());
    ASSERT_THROW(DataSourceClientContainer("sqlite3", config),
                 DataSourceConfigError);

    config->set("class", Element::create(1));
    ASSERT_THROW(DataSourceClientContainer("sqlite3", config),
                 DataSourceConfigError);

    config->set("class", Element::create("FOO"));
    ASSERT_THROW(DataSourceClientContainer("sqlite3", config),
                 DataSourceConfigError);

    config->set("class", Element::create("IN"));
    ASSERT_THROW(DataSourceClientContainer("sqlite3", config),
                 DataSourceConfigError);

    config->set("file", ElementPtr());
    ASSERT_THROW(DataSourceClientContainer("sqlite3", config),
                 DataSourceConfigError);

    config->set("file", Element::create(1));
    ASSERT_THROW(DataSourceClientContainer("sqlite3", config),
                 DataSourceConfigError);

    config->set("file", Element::create("/foo/bar/doesnotexist"));
    ASSERT_THROW(DataSourceClientContainer("sqlite3", config),
                 SQLite3Error);
}


TEST(FactoryTest, sqlite3ClientBadConfig3) {
    ElementPtr config;
    ASSERT_THROW(DataSourceClientContainer("sqlite3", config),
                 DataSourceConfigError);

    config = Element::create("asdf");
    ASSERT_THROW(DataSourceClientContainer("sqlite3", config),
                 DataSourceConfigError);

    config = Element::createMap();
    ASSERT_THROW(DataSourceClientContainer("sqlite3", config),
                 DataSourceConfigError);

    config->set("class", ElementPtr());
    ASSERT_THROW(DataSourceClientContainer("sqlite3", config),
                 DataSourceConfigError);

    config->set("class", Element::create(1));
    ASSERT_THROW(DataSourceClientContainer("sqlite3", config),
                 DataSourceConfigError);

    config->set("class", Element::create("FOO"));
    ASSERT_THROW(DataSourceClientContainer("sqlite3", config),
                 DataSourceConfigError);

    config->set("class", Element::create("IN"));
    ASSERT_THROW(DataSourceClientContainer("sqlite3", config),
                 DataSourceConfigError);

    config->set("file", ElementPtr());
    ASSERT_THROW(DataSourceClientContainer("sqlite3", config),
                 DataSourceConfigError);

    config->set("file", Element::create(1));
    ASSERT_THROW(DataSourceClientContainer("sqlite3", config),
                 DataSourceConfigError);

    config->set("file", Element::create("/foo/bar/doesnotexist"));
    ASSERT_THROW(DataSourceClientContainer("sqlite3", config),
                 SQLite3Error);

    config->set("file", Element::create("/tmp/some_file.sqlite3"));
    DataSourceClientContainer dsc("sqlite3", config);
}
} // end anonymous namespace


// Copyright (C) 2010  Internet Systems Consortium, Inc. ("ISC")
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

// BEGIN_HEADER_GUARD

#include <string>

#include <dns/rdata.h>

// BEGIN_BUNDY_NAMESPACE

// BEGIN_COMMON_DECLARATIONS
// END_COMMON_DECLARATIONS

// BEGIN_RDATA_NAMESPACE

class A : public Rdata {
public:
    // BEGIN_COMMON_MEMBERS
    // END_COMMON_MEMBERS

    //We can use the default destructor.
    //virtual ~A() {}
    // notyet:
    //const struct in_addr& getAddress() const { return (addr_); }
private:
    uint32_t addr_;             // raw IPv4 address (network byte order)
};
// END_RDATA_NAMESPACE
// END_BUNDY_NAMESPACE
// END_HEADER_GUARD

// Local Variables: 
// mode: c++
// End: 

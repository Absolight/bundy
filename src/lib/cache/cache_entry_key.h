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

#ifndef CACHE_ENTRY_KEY_H
#define CACHE_ENTRY_KEY_H

#include <string>
#include <dns/name.h>
#include <dns/rrtype.h>

namespace bundy {
namespace cache {

/// \brief Entry Name Generation Functions
///
/// Generate the name for message/rrset entries.
///
/// Concatenates the string representation of the Name and the
/// string representation of the type number.
///
/// Note: the returned name is a text string, not wire format.
/// eg. if name is 'example.com.', type is 'A', the return
/// value is 'example.com.1'
///
/// \param name The Name to create a text entry for
/// \param type The RRType to create a text entry for
/// \return return the entry name.
const std::string
genCacheEntryName(const bundy::dns::Name& name, const bundy::dns::RRType& type);

///
/// \overload
///
/// \param namestr A string representation of a DNS Name
/// \param type The value of a DNS RRType
const std::string
genCacheEntryName(const std::string& namestr, const uint16_t type);

} // namespace cache
} // namespace bundy

#endif // CACHE_ENTRY_KEY_H


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

#include <config.h>

#include <dns/name.h>
#include <dns/messagerenderer.h>
#include <dns/rdata.h>
#include <dns/rdataclass.h>
#include <dns/rdata/generic/detail/char_string.h>
#include <exceptions/exceptions.h>

#include <string>
#include <boost/lexical_cast.hpp>

using namespace std;
using boost::lexical_cast;
using namespace bundy::util;
using namespace bundy::dns;

// BEGIN_BUNDY_NAMESPACE
// BEGIN_RDATA_NAMESPACE

class NAPTRImpl {
public:
    NAPTRImpl() : replacement(".") {}

    NAPTRImpl(InputBuffer& buffer, size_t rdata_len) : replacement(".") {
        if (rdata_len < 4 || buffer.getLength() - buffer.getPosition() < 4) {
            bundy_throw(bundy::dns::DNSMessageFORMERR, "Error in parsing "
                      "NAPTR RDATA wire format: insufficient length ");
        }
        order = buffer.readUint16();
        preference = buffer.readUint16();
        rdata_len -= 4;

        rdata_len -= detail::bufferToCharString(buffer, rdata_len, flags);
        rdata_len -= detail::bufferToCharString(buffer, rdata_len, services);
        rdata_len -= detail::bufferToCharString(buffer, rdata_len, regexp);
        replacement = Name(buffer);
        if (rdata_len < 1) {
            bundy_throw(bundy::dns::DNSMessageFORMERR, "Error in parsing "
                      "NAPTR RDATA wire format: missing replacement name");
        }
        rdata_len -= replacement.getLength();

        if (rdata_len != 0) {
            bundy_throw(bundy::dns::DNSMessageFORMERR, "Error in parsing " <<
                      "NAPTR RDATA: bytes left at end: " <<
                      static_cast<int>(rdata_len));
        }
    }

    NAPTRImpl(const std::string& naptr_str) : replacement(".") {
        std::istringstream ss(naptr_str);
        MasterLexer lexer;
        lexer.pushSource(ss);

        try {
            parseNAPTRData(lexer);
            // Should be at end of data now
            if (lexer.getNextToken(MasterToken::QSTRING, true).getType() !=
                MasterToken::END_OF_FILE) {
                bundy_throw(InvalidRdataText,
                          "Invalid NAPTR text format: too many fields.");
            }
        } catch (const MasterLexer::LexerError& ex) {
            bundy_throw(InvalidRdataText, "Failed to construct NAPTR RDATA from "
                                        << naptr_str << "': " << ex.what());
        }
    }

    NAPTRImpl(MasterLexer& lexer) : replacement(".")
    {
        parseNAPTRData(lexer);
    }

private:
    void
    parseNAPTRData(MasterLexer& lexer) {
        MasterToken token = lexer.getNextToken(MasterToken::NUMBER);
        if (token.getNumber() > 65535) {
            bundy_throw(InvalidRdataText,
                      "Invalid NAPTR text format: order out of range: "
                      << token.getNumber());
        }
        order = token.getNumber();
        token = lexer.getNextToken(MasterToken::NUMBER);
        if (token.getNumber() > 65535) {
            bundy_throw(InvalidRdataText,
                      "Invalid NAPTR text format: preference out of range: "
                      << token.getNumber());
        }
        preference = token.getNumber();

        token = lexer.getNextToken(MasterToken::QSTRING);
        stringToCharString(token.getStringRegion(), flags);
        token = lexer.getNextToken(MasterToken::QSTRING);
        stringToCharString(token.getStringRegion(), services);
        token = lexer.getNextToken(MasterToken::QSTRING);
        stringToCharString(token.getStringRegion(), regexp);

        token = lexer.getNextToken(MasterToken::STRING);
        replacement = Name(token.getString());
    }


public:
    uint16_t order;
    uint16_t preference;
    detail::CharString flags;
    detail::CharString services;
    detail::CharString regexp;
    Name replacement;
};

NAPTR::NAPTR(InputBuffer& buffer, size_t rdata_len) :
    impl_(new NAPTRImpl(buffer, rdata_len))
{}

NAPTR::NAPTR(const std::string& naptr_str) : impl_(new NAPTRImpl(naptr_str))
{}

NAPTR::NAPTR(MasterLexer& lexer, const Name*,
             MasterLoader::Options, MasterLoaderCallbacks&) :
    impl_(new NAPTRImpl(lexer))
{}

NAPTR::NAPTR(const NAPTR& naptr) :  Rdata(),
                                    impl_(new NAPTRImpl(*naptr.impl_))
{}

NAPTR&
NAPTR::operator=(const NAPTR& source)
{
    impl_.reset(new NAPTRImpl(*source.impl_));
    return (*this);
}

NAPTR::~NAPTR() {
}

void
NAPTR::toWire(OutputBuffer& buffer) const {
    toWireHelper(buffer);
    impl_->replacement.toWire(buffer);
}

void
NAPTR::toWire(AbstractMessageRenderer& renderer) const {
    toWireHelper(renderer);
    // Type NAPTR is not "well-known", and name compression must be disabled
    // per RFC3597.
    renderer.writeName(impl_->replacement, false);
}

string
NAPTR::toText() const {
    string result;
    result += lexical_cast<string>(impl_->order);
    result += " ";
    result += lexical_cast<string>(impl_->preference);
    result += " \"";
    result += detail::charStringToString(impl_->flags);
    result += "\" \"";
    result += detail::charStringToString(impl_->services);
    result += "\" \"";
    result += detail::charStringToString(impl_->regexp);
    result += "\" ";
    result += impl_->replacement.toText();
    return (result);
}

int
NAPTR::compare(const Rdata& other) const {
    const NAPTR other_naptr = dynamic_cast<const NAPTR&>(other);

    if (impl_->order < other_naptr.impl_->order) {
        return (-1);
    } else if (impl_->order > other_naptr.impl_->order) {
        return (1);
    }

    if (impl_->preference < other_naptr.impl_->preference) {
        return (-1);
    } else if (impl_->preference > other_naptr.impl_->preference) {
        return (1);
    }

    const int fcmp = detail::compareCharStrings(impl_->flags,
                                                other_naptr.impl_->flags);
    if (fcmp != 0) {
        return (fcmp);
    }

    const int scmp = detail::compareCharStrings(impl_->services,
                                                other_naptr.impl_->services);
    if (scmp != 0) {
        return (scmp);
    }

    const int rcmp = detail::compareCharStrings(impl_->regexp,
                                                other_naptr.impl_->regexp);
    if (rcmp != 0) {
        return (rcmp);
    }

    return (compareNames(impl_->replacement, other_naptr.impl_->replacement));
}

uint16_t
NAPTR::getOrder() const {
    return (impl_->order);
}

uint16_t
NAPTR::getPreference() const {
    return (impl_->preference);
}

const std::string
NAPTR::getFlags() const {
    return (detail::charStringToString(impl_->flags));
}

const std::string
NAPTR::getServices() const {
    return (detail::charStringToString(impl_->services));
}

const std::string
NAPTR::getRegexp() const {
    return (detail::charStringToString(impl_->regexp));
}

const Name&
NAPTR::getReplacement() const {
    return (impl_->replacement);
}

template <typename T>
void
NAPTR::toWireHelper(T& outputer) const {
    outputer.writeUint16(impl_->order);
    outputer.writeUint16(impl_->preference);

    outputer.writeData(&impl_->flags[0], impl_->flags.size());
    outputer.writeData(&impl_->services[0], impl_->services.size());
    outputer.writeData(&impl_->regexp[0], impl_->regexp.size());
}

// END_RDATA_NAMESPACE
// END_BUNDY_NAMESPACE

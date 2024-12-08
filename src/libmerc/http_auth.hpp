// http_auth.hpp
//
// HTTP Authorization/Authentication

#ifndef HTTP_AUTH_HPP
#define HTTP_AUTH_HPP

#include "datum.h"
#include "base64.h"
#include "http.h"       // for LWS

class scheme : public datum {
public:
    scheme(struct datum& d) {
        datum::parse_up_to_delim(d, ' ');
    }

    /// the enumeration `type` identifies Authorization scheme types;
    /// only Bearer, Basic, and Digest are currently supported
    ///
    /// The full set of schemes is laid out at
    /// https://www.iana.org/assignments/http-authschemes/http-authschemes.xhtml
    ///
    enum type {
        unknown,
        basic,
        digest,
        bearer,
    };

    static const char *type_get_name(enum type t) {
        switch(t) {
        case basic: return "basic";
        case digest: return "digest";
        case bearer: return "bearer";
        case unknown:
        default:
            ;
        }
        return "unknown";
    }

    scheme::type get_type() const {
        //
        // TODO: technically, this function should use
        // case-insensitive comparisons
        //
        if (this->cmp(std::array<uint8_t,5>{'B', 'a', 's', 'i', 'c'})) {
            return type::basic;
        }
        if (this->cmp(std::array<uint8_t,6>{'B', 'e', 'a', 'r', 'e', 'r'})) {
            return type::bearer;
        }
        if (this->cmp(std::array<uint8_t,6>{'D', 'i', 'g', 'e', 's', 't'})) {
            return type::digest;
        }
        return unknown;
    }

};

class authorization {
    scheme auth_scheme;
    LWS lws;
    datum auth_param;

public:

    /// construct an `authorization` object from a \ref datum (not a `datum` reference)
    ///
    authorization(datum d) :
        auth_scheme{d},
        lws{d},
        auth_param{d}
    { }

    bool is_valid() const { return auth_param.is_not_null(); }

    void write_json(json_object &o) const {
        if (!is_valid()) { return; }
        json_object auth_json{o, "authorization"};
        scheme::type schemetype = auth_scheme.get_type();
        if (schemetype == scheme::type::basic || schemetype == scheme::type::bearer) {
            uint8_t outbuf[128];
            int outlen = base64::decode(outbuf, sizeof(outbuf), auth_param.data, auth_param.length());
            json_object scheme_json{auth_json, auth_scheme.type_get_name(schemetype)};
            scheme_json.print_key_json_string("param", outbuf, outlen);
            scheme_json.close();
        } else {
            json_object scheme_json{auth_json, auth_scheme.type_get_name(schemetype)};
            scheme_json.print_key_json_string("param", auth_param);
            scheme_json.close();
        }
        auth_json.close();
    }

};

#endif

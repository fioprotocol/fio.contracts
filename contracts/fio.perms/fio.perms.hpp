/** FIO permissions
 *  Description: see fio.perm.cpp.
 *  @author Ed Rotthoff
 *  @file fio.perms.hpp
 *  @license FIO Foundation ( https://github.com/fioprotocol/fio/blob/master/LICENSE )
 */

#pragma once

#include <eosiolib/eosio.hpp>
#include <eosiolib/singleton.hpp>
#include <eosiolib/asset.hpp>

#include <string>

using std::string;

namespace fioio {

    using namespace eosio;

    const static string REGISTER_ADDRESS_ON_DOMAIN_PERMISSION_NAME = "register_address_on_domain";
    const static string REGISTER_ADDRESS_ON_DOMAIN_OBJECT_TYPE = "domain";

    struct [[eosio::action]] permission_info {

        uint64_t id = 0;
        // this is a string whose acceptable values should be defined in this file, we use "domain" for FIP-40.
        string object_type = "";
        uint128_t object_type_hash = 0;
        //this is the name of the object being controlled in state, for FIP-40 this will be the name of the domain
        //being permissed to create new addresses.
        string object_name = "";
        uint128_t object_name_hash = 0;
        //this is the name of the permission, these values should be constants defined in this file.
        //for FIP-40 we will use register_address_on_domain as the value.
        string permission_name = "";
        uint128_t permission_name_hash = 0;
        //by convention we will store the hashed value of the following concatination in this field to provide a
        //unique search key by object_type, object_name, and permission_name
        uint128_t permission_control_hash = 0;
        uint64_t owner_account = 0;
        //this field can contain any string based info that is useful for the permission.
        //it shouldbe json based. for FIP-40 this is unused.
        string  auxilliary_info = "";


        uint64_t primary_key() const { return id; }
        uint128_t by_object_type_hash() const { return object_type_hash; }
        uint128_t by_object_name_hash() const { return object_name_hash; }
        uint128_t by_permission_name_hash() const { return permission_name_hash; }
        uint128_t by_permission_control_hash() const { return permission_control_hash; }
        uint64_t by_owner_account() const { return owner_account; }


        EOSLIB_SERIALIZE(permission_info, (id)(object_type)(object_type_hash)(object_name)(object_name_hash)
                (permission_name)(permission_name_hash)(permission_control_hash)(owner_account)(auxilliary_info))
    };
    //this state table contains information relating to the permissions that are granted in the FIO protocol
    //please examine fio.perms.cpp for details relating to FIO permissions.
    typedef multi_index<"permissions"_n, permission_info,
            indexed_by<"byobjtype"_n, const_mem_fun < permission_info, uint128_t, &permission_info::by_object_type_hash>>,
            indexed_by<"byobjname"_n, const_mem_fun < permission_info, uint128_t, &permission_info::by_object_name_hash>>,
            indexed_by<"bypermname"_n, const_mem_fun < permission_info, uint128_t, &permission_info::by_permission_name_hash>>,
            indexed_by<"bypermctrl"_n, const_mem_fun < permission_info, uint128_t, &permission_info::by_permission_control_hash>>,
            indexed_by<"byowner"_n, const_mem_fun < permission_info, uint64_t, &permission_info::by_owner_account>>
    >
    permissions_table;





struct [[eosio::action]] access_info {

    uint64_t id = 0;
    uint64_t permission_id = 0;
    uint64_t grantee_account = 0;
    //this is the hashed value of the string concatination of grantee account, permission id
    uint128_t access_hash = 0;


    uint64_t primary_key() const { return id; }
    uint64_t by_permission_id() const { return permission_id; }
    uint64_t by_grantee_account() const { return grantee_account; }
    uint128_t by_access_hash() const { return access_hash; }


    EOSLIB_SERIALIZE(access_info, (id)(permission_id)(grantee_account)(access_hash))
};
//this state table contains information relating to the acesses that are granted in the FIO protocol
//please examine fio.perms.cpp for details relating to FIO permissions.
typedef multi_index<"accesses"_n, access_info,
        indexed_by<"bypermid"_n, const_mem_fun < access_info, uint64_t, &access_info::by_permission_id>>,
        indexed_by<"bygrantee"_n, const_mem_fun < access_info, uint64_t, &access_info::by_grantee_account>>,
        indexed_by<"byaccess"_n, const_mem_fun < access_info, uint128_t, &access_info::by_access_hash>>
>
access_table;



}

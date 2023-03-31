/** FIO permissions contract
 *  Description:
 *
 *
 *        We will introduce a notion of a permission, a permission is a definition of information that provides some control
 *       and/or access to objects in state which are owned by the account that creates the permission.
 *       Permissions definitions will be extensible within the FIO protocol. new permissions can be added into
 *       the protocols contracts using the permmissions tables in this contract.
 *
 *
 *       The following process will be used for the definition and integration of new permissions in the FIO protocols:
 *               Step 1:  define the permission desired.
 *               Step 2: modify the fio.contracts affected by the new access to enforce and integrate the new permission.
 *               Step 3: rollout the new permission into testnet and main net using the following actions
 *                     3.1.   rollout the new version of the contracts supporting the new permission.
 *                     3.2   FIO user accounts begin using the permission as indicated in the spec.
 *
 *      The following vernacular is used throughout our design:
 *      Permission –  the name of the permission,
 *      Permission info -- the object type that is to be controlled, the name of the object to be controlled,
 *      the owning account, and also including all parameterized data used by the permission according to the
 *      business logic required (such as access levels, or other abstractions that can be set when an
 *      account grants the permission).
 *      Permission Auxilliary Info – the json definition of all of the parameterized data used by a given permission that is unique for a permission.
 *      for FIP-40 no additional data is necessary. this field provides extensibility such that we can introduce new
 *      or novel parameters and dials used by a new permission if this is necessary.
 *      Grantor – the granting/owning account of the object that relates to the permission.
 *      Object – the object that is being access controlled by a permission (for FIP-40 this is the domain).
 *      Grantee – the non grantor account that is given a permission.
 *      Access -- an account has access to a permission when a permission is granted to an account.
 *
 *
 *
 *
 *
 *
 *  @author  Ed Rotthoff
 *  @file fio.perms.cpp
 *  @license FIO Foundation ( https://github.com/fioprotocol/fio/blob/master/LICENSE )
 */

#include "fio.perms.hpp"
#include <fio.address/fio.address.hpp>
#include <fio.fee/fio.fee.hpp>
#include <fio.common/fio.common.hpp>
#include <fio.common/fiotime.hpp>
#include <fio.token/include/fio.token/fio.token.hpp>
#include <eosiolib/asset.hpp>

namespace fioio {

    class [[eosio::contract("FioPermissions")]]  FioPermissions : public eosio::contract {

    private:

        domains_table     domains;
        fionames_table    fionames;
        fiofee_table      fiofees;
        eosio_names_table accountmap;
        permissions_table permissions;
        access_table      accesses;
        config            appConfig;



    public:
        using contract::contract;

        FioPermissions(name s, name code, datastream<const char *> ds) : contract(s, code, ds),
                                                                         permissions(_self,_self.value),
                                                                         accesses(_self,_self.value),
                                                                         domains(AddressContract, AddressContract.value),
                                                                        fionames(AddressContract, AddressContract.value),
                                                                        fiofees(FeeContract, FeeContract.value),
                                                                        accountmap(_self, _self.value){
            configs_singleton configsSingleton(FeeContract, FeeContract.value);
            appConfig = configsSingleton.get_or_default(config());
        }


        /*
         * This action will check if a permission exists for the specified arguments, if it does not
         * yet exist in the permissions table a new record will be added, the accesses table will also
         * be updated to indicate the grantee account access that has been granted. please see the code
         * for error logic and parameter descriptions.
         */
        [[eosio::action]]
        void
        addperm(const name &grantee_account,
                const string &permission_name,   //one permission is permitted register_address_on_domain
                const string &permission_info,   //this is empty for FIP-40, an extensibility field for the future.
                const string &object_name,       //the name of the fio domain
                const int64_t &max_fee,
                const string &tpid,
                const name &actor
                ) {


            print("addperm --      called. \n");

            require_auth(actor);
            string useperm = makeLowerCase(permission_name);


            fio_400_assert(permission_name.length()  > 0, "permission_name", permission_name,
                           "Permission name is invalid", ErrorInvalidPermissionName);
            //  error if permission name is not the expected name register_address_on_domain.
            //one permission name is integrated for fip 40, modify this logic for any new permission names
            //being supported
            fio_400_assert(useperm.compare(REGISTER_ADDRESS_ON_DOMAIN_PERMISSION_NAME) == 0, "permission_name", permission_name,
                           "Permission name is invalid", ErrorInvalidPermissionName);

            // error if permission info is not empty.
            fio_400_assert(permission_info.size()  == 0, "permission_info", permission_info,
                           "Permission info is invalid", ErrorInvalidPermissionInfo);
            // error if object name is not * or is not in the domains table
            fio_400_assert(object_name.size()  > 0, "object_name", object_name,
                           "Object name is invalid", ErrorInvalidObjectName);

            //verify domain name, and that domain is owned by the actor account.
            FioAddress fa;
            getFioAddressStruct(object_name, fa);

            fio_400_assert(fa.domainOnly, "domonlyobject_name", object_name, "Invalid object name",
                           ErrorInvalidObjectName);

            const uint128_t domainHash = string_to_uint128_hash(fa.fiodomain.c_str());
            auto domainsbyname = domains.get_index<"byname"_n>();
            auto domains_iter = domainsbyname.find(domainHash);

            fio_400_assert(domains_iter != domainsbyname.end(), "domnotfound object_name", object_name,
                           "Invalid object name",
                           ErrorInvalidObjectName);

            //add 30 days to the domain expiration, this call will work until 30 days past expire.
            const uint32_t domain_expiration = get_time_plus_seconds(domains_iter->expiration, SECONDS30DAYS);

            const uint32_t present_time = now();
            fio_400_assert(present_time <= domain_expiration, "domexpired object_name", object_name, "Invalid object name",
                           ErrorInvalidObjectName);

            //check domain owner is actor
            fio_400_assert((actor.value == domains_iter->account), "actornotowner object_name", object_name,
                           "Invalid object name", ErrorInvalidObjectName);


            //check grantee exists.
            fio_400_assert(is_account(grantee_account), "grantee_account", grantee_account.to_string(),
                           "grantee account is invalid", ErrorInvalidGranteeAccount);

            fio_400_assert((grantee_account.value != actor.value), "grantee_account", grantee_account.to_string(),
                           "grantee account is invalid", ErrorInvalidGranteeAccount);


            //error if the grantee account already has this permission.
            string permcontrol = REGISTER_ADDRESS_ON_DOMAIN_OBJECT_TYPE + object_name + REGISTER_ADDRESS_ON_DOMAIN_PERMISSION_NAME;
            const    uint128_t permcontrolHash = string_to_uint128_hash(permcontrol.c_str());
            auto     accessbyhash              = accesses.get_index<"byaccess"_n>();
            auto     permissionsbycontrolhash  = permissions.get_index<"bypermctrl"_n>();
            auto     permctrl_iter             = permissionsbycontrolhash.find(permcontrolHash);
            uint64_t permid                    = 0;

            if(permctrl_iter == permissionsbycontrolhash.end())
            { //insert the permission
                //one permission name is integrated for fip 40, modify this logic for any new permission names
                //being supported
                string object_type = PERMISSION_OBJECT_TYPE_DOMAIN;
                const string controlv = object_type + object_name + useperm;
                permid = permissions.available_primary_key();
                permissions.emplace(get_self(), [&](struct permission_info &p) {
                    p.id = permid;
                    p.object_type = PERMISSION_OBJECT_TYPE_DOMAIN;
                    p.object_type_hash = string_to_uint128_hash(PERMISSION_OBJECT_TYPE_DOMAIN);
                    p.object_name = object_name;
                    p.object_name_hash = string_to_uint128_hash(object_name);
                    p.permission_name = useperm;
                    p.permission_name_hash = string_to_uint128_hash(useperm);
                    p.permission_control_hash = string_to_uint128_hash(controlv);
                    p.owner_account = actor.value;
                    p.auxilliary_info = "";
                });
            }
            else {
                //get the id for the perm
                permid = permctrl_iter->id;
            }


            string accessctrl           = grantee_account.to_string() + to_string(permid);
            const  uint128_t accessHash = string_to_uint128_hash(accessctrl.c_str());
            auto access_iter            = accessbyhash.find(accessHash);

            fio_400_assert((access_iter == accessbyhash.end() ), "grantee_account", grantee_account.to_string(),
                           "Permission already exists", ErrorPermissionExists);

            //add the record to accesses.
            const uint64_t accessid = accesses.available_primary_key();
            accesses.emplace(get_self(), [&](struct access_info &a) {
                a.id = accessid;
                a.permission_id = permid;
                a.grantee_account = grantee_account.value;
                a.access_hash = accessHash;
            });


            fio_400_assert(max_fee >= 0, "max_fee", to_string(max_fee), "Invalid fee value",
                           ErrorMaxFeeInvalid);
            fio_400_assert(validateTPIDFormat(tpid), "tpid", tpid,
                           "TPID must be empty or valid FIO address",
                           ErrorPubKeyValid);


            //fees
            const uint128_t endpoint_hash = string_to_uint128_hash(ADD_PERMISSION_ENDPOINT);
            auto fees_by_endpoint = fiofees.get_index<"byendpoint"_n>();
            auto fee_iter = fees_by_endpoint.find(endpoint_hash);
            fio_400_assert(fee_iter != fees_by_endpoint.end(), "endpoint_name", ADD_PERMISSION_ENDPOINT,
                           "FIO fee not found for endpoint", ErrorNoEndpoint);
            const uint64_t fee_amount = fee_iter->suf_amount;
            const uint64_t fee_type = fee_iter->type;

            fio_400_assert(fee_type == 0, "fee_type", to_string(fee_type),
                           "unexpected fee type for endpoint transfer_fio_domain, expected 0",
                           ErrorNoEndpoint);

            fio_400_assert(max_fee >= (int64_t) fee_amount, "max_fee", to_string(max_fee),
                           "Fee exceeds supplied maximum.",
                           ErrorMaxFeeExceeded);

            fio_fees(actor, asset(fee_amount, FIOSYMBOL), ADD_PERMISSION_ENDPOINT);
            processbucketrewards(tpid, fee_amount, get_self(), actor);

            if (fee_amount > 0) {
                INLINE_ACTION_SENDER(eosiosystem::system_contract, updatepower)
                        (SYSTEMACCOUNT, {{_self, "active"_n}},
                         {actor, true}
                        );
            }

            //ram bump
            if (ADDPERMISSIONRAM > 0) {
                action(
                        permission_level{SYSTEMACCOUNT, "active"_n},
                        "eosio"_n,
                        "incram"_n,
                        std::make_tuple(actor, ADDPERMISSIONRAMBASE + (ADDPERMISSIONRAM * permission_info.size()))
                ).send();
            }
            const string response_string = "{\"status\": \"OK\", \"fee_collected\" : "+ to_string(fee_amount) +"}";
            send_response(response_string.c_str());

        }

        [[eosio::action]]
        void
        remperm(const name &grantee_account,
                const string &permission_name,
                const string &object_name,
                const int64_t &max_fee,
                const string &tpid,
                const name &actor
        ) {


            print("remperm --      called. \n");

            require_auth(actor);


            fio_400_assert(permission_name.length()  > 0, "permission_name", permission_name,
                           "Permission name is invalid", ErrorInvalidPermissionName);

            string useperm = makeLowerCase(permission_name);

            // error if permission name is not the expected name register_address_on_domain.
            fio_400_assert(useperm.compare(REGISTER_ADDRESS_ON_DOMAIN_PERMISSION_NAME) == 0, "permission_name", permission_name,
                           "Permission name is invalid", ErrorInvalidPermissionName);

             // error if object name is not * or is not in the domains table
            fio_400_assert(object_name.size()  > 0, "object_name", object_name,
                           "Object name is invalid", ErrorInvalidObjectName);

            //verify domain name, and that domain is owned by the actor account.
            FioAddress fa;
            getFioAddressStruct(object_name, fa);

            fio_400_assert(fa.domainOnly, "object_name", object_name, "Invalid object name",
                           ErrorInvalidObjectName);

            const uint128_t domainHash = string_to_uint128_hash(fa.fiodomain.c_str());
            auto  domainsbyname        = domains.get_index<"byname"_n>();
            auto  domains_iter         = domainsbyname.find(domainHash);

            fio_400_assert(domains_iter != domainsbyname.end(), "object_name", object_name,
                           "Invalid object name",
                           ErrorInvalidObjectName);

            //add 30 days to the domain expiration, this call will work until 30 days past expire.
            const uint32_t domain_expiration = get_time_plus_seconds(domains_iter->expiration, SECONDS30DAYS);
            const uint32_t present_time      = now();

            fio_400_assert(present_time <= domain_expiration, "object_name", object_name, "Invalid object name",
                           ErrorInvalidObjectName);

            //check domain owner is actor
            fio_400_assert((actor.value == domains_iter->account), "object_name", object_name,
                           "Invalid object name", ErrorInvalidObjectName);


            //check grantee exists.
            fio_400_assert(is_account(grantee_account), "grantee_account", grantee_account.to_string(),
                           "grantee account is invalid", ErrorInvalidGranteeAccount);

            fio_400_assert((grantee_account.value != actor.value), "grantee_account", grantee_account.to_string(),
                           "grantee account is invalid", ErrorInvalidGranteeAccount);


            //error if the grantee account already has this permission.
            string permcontrol = REGISTER_ADDRESS_ON_DOMAIN_OBJECT_TYPE + object_name + REGISTER_ADDRESS_ON_DOMAIN_PERMISSION_NAME;

            const uint128_t permcontrolHash = string_to_uint128_hash(permcontrol.c_str());

            auto permissionsbycontrolhash = permissions.get_index<"bypermctrl"_n>();
            auto permctrl_iter = permissionsbycontrolhash.find(permcontrolHash);
            if (permctrl_iter != permissionsbycontrolhash.end() ){
                //get the id and look in access, remove it if its there, error if not there
                uint64_t permid               = permctrl_iter->id;
                string   accessctrl           = grantee_account.to_string() + to_string(permid);
                const    uint128_t accessHash = string_to_uint128_hash(accessctrl.c_str());
                auto     accessbyhash         = accesses.get_index<"byaccess"_n>();
                auto     access_iter          = accessbyhash.find(accessHash);

                fio_400_assert((access_iter != accessbyhash.end() ), "grantee_account", grantee_account.to_string(),
                               "Permission not found", ErrorPermissionExists);
                accessbyhash.erase(access_iter);
                //do one more check for this access by permission id, if no results then
                //remove from permissions.
                auto accessbypermid       = accesses.get_index<"bypermid"_n>();
                auto accessbyperm_iter    = accessbypermid.find(permid);
                if(accessbyperm_iter == accessbypermid.end()){
                    //no accounts with this access left, remove the permission.
                    permissionsbycontrolhash.erase(permctrl_iter);
                }
            }else{
                //cant find access by control hash. permission not found
                fio_400_assert((permctrl_iter != permissionsbycontrolhash.end()), "grantee_account", grantee_account.to_string(),
                               "Permission not found", ErrorPermissionExists);
            }

            fio_400_assert(max_fee >= 0, "max_fee", to_string(max_fee), "Invalid fee value",
                           ErrorMaxFeeInvalid);
            fio_400_assert(validateTPIDFormat(tpid), "tpid", tpid,
                           "TPID must be empty or valid FIO address",
                           ErrorPubKeyValid);

            //fees
            const uint128_t endpoint_hash    = string_to_uint128_hash(REMOVE_PERMISSION_ENDPOINT);
            auto            fees_by_endpoint = fiofees.get_index<"byendpoint"_n>();
            auto            fee_iter         = fees_by_endpoint.find(endpoint_hash);

            fio_400_assert(fee_iter != fees_by_endpoint.end(), "endpoint_name", REMOVE_PERMISSION_ENDPOINT,
                           "FIO fee not found for endpoint", ErrorNoEndpoint);

            const uint64_t fee_amount = fee_iter->suf_amount;
            const uint64_t fee_type   = fee_iter->type;

            fio_400_assert(fee_type == 0, "fee_type", to_string(fee_type),
                           "unexpected fee type for endpoint remove permission, expected 0",
                           ErrorNoEndpoint);

            fio_400_assert(max_fee >= (int64_t) fee_amount, "max_fee", to_string(max_fee),
                           "Fee exceeds supplied maximum.",
                           ErrorMaxFeeExceeded);

            fio_fees(actor, asset(fee_amount, FIOSYMBOL), REMOVE_PERMISSION_ENDPOINT);
            processbucketrewards(tpid, fee_amount, get_self(), actor);

            if (fee_amount > 0) {
                INLINE_ACTION_SENDER(eosiosystem::system_contract, updatepower)
                        (SYSTEMACCOUNT, {{_self, "active"_n}},
                         {actor, true}
                        );
            }
            const string response_string = "{\"status\": \"OK\", \"fee_collected\" : "+ to_string(fee_amount) +"}";
            send_response(response_string.c_str());
        }
    };

    EOSIO_DISPATCH(FioPermissions, (addperm)(remperm))
}

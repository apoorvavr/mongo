/**
*    Copyright (C) 2012 10gen Inc.
*
*    This program is free software: you can redistribute it and/or  modify
*    it under the terms of the GNU Affero General Public License, version 3,
*    as published by the Free Software Foundation.
*
*    This program is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*    GNU Affero General Public License for more details.
*
*    You should have received a copy of the GNU Affero General Public License
*    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <map>
#include <string>

#include "mongo/base/disallow_copying.h"
#include "mongo/db/auth/acquired_privilege.h"
#include "mongo/db/auth/action_set.h"
#include "mongo/db/auth/action_type.h"
#include "mongo/db/auth/privilege.h"
#include "mongo/db/auth/principal_name.h"
#include "mongo/util/string_map.h"

namespace mongo {

    /**
     * A collection of privileges describing which authenticated principals bestow the client the
     * ability to perform various actions on specific resources.  Since every privilege comes from
     * an authenticated principal, removing that principal removes all privileges granted by that
     * principal.
     *
     * Resources are arranged hierarchically, with a wildcard resource,
     * PrivilegeSet::WILDCARD_RESOURCE, matching any resource.  In the current implementation, the
     * only two levels of the hierarchy are the wildcard and one level below, which is analagous to
     * the name of a database.  It is future work to support collection or other sub-database
     * resources.
     *
     * This class does not do any locking/synchronization, the consumer will be responsible for
     * synchronizing access.
     */
    class PrivilegeSet {
        MONGO_DISALLOW_COPYING(PrivilegeSet);
    public:
        static const std::string WILDCARD_RESOURCE;

        PrivilegeSet();
        ~PrivilegeSet();

        /**
         * Adds the specified privilege to the set, associating it with the named principal.
         *
         * The privilege should be on a specific resource, or on the WILDCARD_RESOURCE.
         */
        void grantPrivilege(const Privilege& privilege, const PrincipalName& authorizingPrincipal);

        /**
         * Adds the specified privileges to the set, associating them with the named principal.
         */
        void grantPrivileges(const std::vector<Privilege>& privileges,
                             const PrincipalName& authorizingPrincipal);

        /**
         * Removes from the set all privileges associated with the given principal.
         *
         * If multiple princpals enable the same privilege, the set will continue to
         * contain those privileges until all authorizing principals have had their
         * privileges revoked from the set.
         */
        void revokePrivilegesFromPrincipal(const PrincipalName& principal);

        /**
         * Returns true if the set authorizes "desiredPrivilege".
         *
         * The set is considered to authorize "desiredPrivilege" if each action in
         * "desiredPrivilege" is satisfied either on "desiredPrivilege.getResource()" or on
         * WILDCARD_RESOURCE.
         */
        bool hasPrivilege(const Privilege& desiredPrivilege);

        /**
         * Same as hasPrivilege, except checks all the privileges in a vector.
         */
        bool hasPrivileges(const std::vector<Privilege>& desiredPrivileges);

    private:

        /**
         * Information about privileges held on a resource.
         *
         * Instances are stored in the _byResource map, and accelerate the fast path of
         * hasPrivilege().  Privilege revocations via revokePrivilegesFromPrincipal() can make these
         * entries invalid, at which point they are marked "dirty".  Dirty entries are rebuilt via
         * _rebuildEntry(), below, during execution of hasPrivilege().
         */
        class ResourcePrivilegeCacheEntry {
        public:
            ResourcePrivilegeCacheEntry() : actions(), dirty(false) {}

            // All actions enabled on the associated resource, provided that "dirty" is false.
            ActionSet actions;

            // False if this data is consistent with the full privilege information, stored in the
            // _byPrincipal map.
            bool dirty;
        };

        /**
         * Type of map from resource names to authorized actions.
         */
        typedef StringMap<ResourcePrivilegeCacheEntry> ResourcePrivilegeCache;

        /**
         * Type of map from principal identity to information about the principal's privileges.  The
         * values in the map are themselves maps from resource names to associated actions.
         */
        typedef std::map<PrincipalName, StringMap<ActionSet> > PrincipalPrivilegeMap;

        void _rebuildEntry(const StringData& resource, ResourcePrivilegeCacheEntry* summary);

        ResourcePrivilegeCacheEntry* _lookupEntry(const StringData& resource);
        ResourcePrivilegeCacheEntry* _lookupOrInsertEntry(const StringData& resource);

        // Information about privileges available on all resources.
        ResourcePrivilegeCacheEntry _globalPrivilegeEntry;

        // Cache of privilege information, by resource.
        ResourcePrivilegeCache _byResource;

        // Directory of privilege information, by principal.
        PrincipalPrivilegeMap _byPrincipal;
    };

} // namespace mongo

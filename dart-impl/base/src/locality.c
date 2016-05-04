/**
 * \file dash/dart/base/locality.c
 */

/*
 * Include config and utmpx.h first to prevent previous include of utmpx.h
 * without _GNU_SOURCE in included headers:
 */
#include <dash/dart/base/config.h>
#ifdef DART__PLATFORM__LINUX
#  define _GNU_SOURCE
#  include <utmpx.h>
#endif

#include <dash/dart/base/macro.h>
#include <dash/dart/base/logging.h>
#include <dash/dart/base/locality.h>
#include <dash/dart/base/logging.h>
#include <dash/dart/base/assert.h>
#include <dash/dart/base/hwinfo.h>

#include <dash/dart/base/internal/host_topology.h>
#include <dash/dart/base/internal/unit_locality.h>
#include <dash/dart/base/internal/domain_locality.h>

#include <dash/dart/if/dart_types.h>
#include <dash/dart/if/dart_locality.h>
#include <dash/dart/if/dart_communication.h>

#include <inttypes.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <sched.h>
#include <limits.h>

/* ======================================================================== *
 * Private Data                                                             *
 * ======================================================================== */

#define DART__BASE__LOCALITY__MAX_TEAM_DOMAINS 32

dart_host_topology_t *
dart__base__locality__host_topology_[DART__BASE__LOCALITY__MAX_TEAM_DOMAINS];

dart_unit_mapping_t *
dart__base__locality__unit_mapping_[DART__BASE__LOCALITY__MAX_TEAM_DOMAINS];

dart_domain_locality_t *
dart__base__locality__global_domain_[DART__BASE__LOCALITY__MAX_TEAM_DOMAINS];

/* ======================================================================== *
 * Init / Finalize                                                          *
 * ======================================================================== */

dart_ret_t dart__base__locality__init()
{
  return dart__base__locality__create(DART_TEAM_ALL);
}

dart_ret_t dart__base__locality__finalize()
{
  for (dart_team_t t = 0; t < DART__BASE__LOCALITY__MAX_TEAM_DOMAINS; ++t) {
    dart__base__locality__delete(t);
  }

  dart_barrier(DART_TEAM_ALL);
  return DART_OK;
}

/* ======================================================================== *
 * Create / Delete                                                          *
 * ======================================================================== */

dart_ret_t dart__base__locality__create(
  dart_team_t team)
{
  DART_LOG_DEBUG("dart__base__locality__create() team(%d)", team);

  dart_hwinfo_t * hwinfo;
  DART_ASSERT_RETURNS(dart_hwinfo(&hwinfo), DART_OK);

  for (int td = 0; td < DART__BASE__LOCALITY__MAX_TEAM_DOMAINS; ++td) {
    dart__base__locality__global_domain_[td] = NULL;
    dart__base__locality__host_topology_[td] = NULL;
  }

  dart_domain_locality_t * team_all_global_domain =
    malloc(sizeof(dart_domain_locality_t));
  dart__base__locality__global_domain_[team] =
    team_all_global_domain;

  /* Initialize the global domain as the root entry in the locality
   * hierarchy: */
  team_all_global_domain->scope         = DART_LOCALITY_SCOPE_GLOBAL;
  team_all_global_domain->level         = 0;
  team_all_global_domain->parent        = NULL;
  team_all_global_domain->num_domains   = 0;
  team_all_global_domain->domains       = NULL;
  team_all_global_domain->hwinfo        = *hwinfo;
  team_all_global_domain->num_units     = 0;
  team_all_global_domain->host[0]       = '\0';
  team_all_global_domain->domain_tag[0] = '.';
  team_all_global_domain->domain_tag[1] = '\0';

  size_t num_units = 0;
  DART_ASSERT_RETURNS(dart_team_size(team, &num_units), DART_OK);
  team_all_global_domain->num_units = num_units;

  team_all_global_domain->unit_ids =
    malloc(num_units * sizeof(dart_unit_t));
  for (size_t u = 0; u < num_units; ++u) {
    team_all_global_domain->unit_ids[u] = u;
  }

  /* Exchange unit locality information between all units: */
  dart_unit_mapping_t * unit_mapping;
  DART_ASSERT_RETURNS(
    dart__base__unit_locality__create(team, &unit_mapping),
    DART_OK);
  dart__base__locality__unit_mapping_[team] = unit_mapping;

  /* Filter unique host names from locality information of all units.
   * Could be further optimized but only runs once during startup. */
  const int max_host_len = DART_LOCALITY_HOST_MAX_SIZE;
  DART_LOG_TRACE("dart__base__locality__create: copying host names");
  /* Copy host names of all units into array: */
  char ** hosts = malloc(sizeof(char *) * num_units);
  for (size_t u = 0; u < num_units; ++u) {
    hosts[u] = malloc(sizeof(char) * max_host_len);
    dart_unit_locality_t * ul;
    DART_ASSERT_RETURNS(
      dart__base__unit_locality__at(unit_mapping, u, &ul),
      DART_OK);
    strncpy(hosts[u], ul->host, max_host_len);
  }

  dart_host_topology_t * topo = malloc(sizeof(dart_host_topology_t));
  DART_ASSERT_RETURNS(
    dart__base__host_topology__create(
      hosts, team, unit_mapping, topo),
    DART_OK);
  dart__base__locality__host_topology_[team] = topo;
  size_t num_nodes = topo->num_nodes;
  DART_LOG_TRACE("dart__base__locality__create: nodes: %d", num_nodes);

  team_all_global_domain->num_nodes = num_nodes;

#ifdef DART_ENABLE_LOGGING
  for (int h = 0; h < topo->num_hosts; ++h) {
    dart_node_units_t * node_units = &topo->node_units[h];
    char * hostname = topo->host_names[h];
    DART_LOG_TRACE("dart__base__locality__create: "
                   "host %s: units:%d level:%d parent:%s", hostname,
                   node_units->num_units, node_units->level,
                   node_units->parent);
    for (int u = 0; u < node_units->num_units; ++u) {
      DART_LOG_TRACE("dart__base__locality__create: %s unit[%d]: %d",
                     hostname, u, node_units->units[u]);
    }
  }
#endif

  /* recursively create locality information of the global domain's
   * sub-domains: */
  DART_ASSERT_RETURNS(
    dart__base__locality__create_subdomains(
      dart__base__locality__global_domain_[team],
      dart__base__locality__host_topology_[team],
      dart__base__locality__unit_mapping_[team]),
    DART_OK);

  DART_LOG_DEBUG("dart__base__locality__create >");
  return DART_OK;
}

dart_ret_t dart__base__locality__delete(
  dart_team_t team)
{
  dart_ret_t ret = DART_OK;

  if (dart__base__locality__global_domain_[team] == NULL &&
      dart__base__locality__host_topology_[team] == NULL &&
      dart__base__locality__unit_mapping_[team]  == NULL) {
    return ret;
  }

  DART_LOG_DEBUG("dart__base__locality__delete() team(%d)", team);

  if (dart__base__locality__global_domain_[team] != NULL) {
    ret = dart__base__locality__domain_delete(
            dart__base__locality__global_domain_[team]);
    if (ret != DART_OK) {
      DART_LOG_ERROR("dart__base__locality__delete ! "
                     "dart__base__locality__domain_delete failed: %d", ret);
      return ret;
    }
    dart__base__locality__global_domain_[team] = NULL;
  }

  if (dart__base__locality__host_topology_[team] != NULL) {
    ret = dart__base__host_topology__delete(
            dart__base__locality__host_topology_[team]);
    if (ret != DART_OK) {
      DART_LOG_ERROR("dart__base__locality__delete ! "
                     "dart__base__host_topology__delete failed: %d", ret);
      return ret;
    }
    dart__base__locality__host_topology_[team] = NULL;
  }

  if (dart__base__locality__unit_mapping_[team] != NULL) {
    ret = dart__base__unit_locality__delete(
            dart__base__locality__unit_mapping_[team]);
    if (ret != DART_OK) {
      DART_LOG_ERROR("dart__base__locality__delete ! "
                     "dart__base__unit_locality__delete failed: %d", ret);
      return ret;
    }
    dart__base__locality__unit_mapping_[team] = NULL;
  }

  DART_LOG_DEBUG("dart__base__locality__delete > team(%d)", team);
  return DART_OK;
}

/* ======================================================================== *
 * Domain Locality                                                          *
 * ======================================================================== */

dart_ret_t dart__base__locality__domain(
  dart_team_t               team,
  const char              * domain_tag,
  dart_domain_locality_t ** locality)
{
  DART_LOG_DEBUG("dart__base__locality__domain() team(%d) domain(%s)",
                 team, domain_tag);

  dart_domain_locality_t * domain =
    dart__base__locality__global_domain_[team];

  /* pointer to dot separator in front of tag part:  */
  char * dot_begin  = strchr(domain_tag, '.');
  /* pointer to begin of tag part: */
  char * part_begin = dot_begin + 1;
  /* Iterate tag (.1.2.3) by parts (1, 2, 3):        */
  int    level     = 0;
  while (dot_begin != NULL && *dot_begin != '\0' && *part_begin != '\0') {
    char * dot_end;
    /* domain tag part converted to int is relative index of child: */
    long   tag_part      = strtol(part_begin, &dot_end, 10);
    int    subdomain_idx = (int)(tag_part);
    if (domain == NULL) {
      /* tree leaf node reached before last tag part: */
      DART_LOG_ERROR("dart__base__locality__domain ! team(%d) domain(%s): "
                     "subdomain at index %d in level %d is undefined",
                     team, domain_tag, subdomain_idx, level);
      return DART_ERR_INVAL;
    }
    if (domain->num_domains <= subdomain_idx) {
      /* child index out of range: */
      DART_LOG_ERROR("dart__base__locality__domain ! team(%d) domain(%s): "
                     "subdomain index %d in level %d is out ouf bounds "
                     "(number of subdomains: %d)",
                     team, domain_tag, subdomain_idx, level,
                     domain->num_domains);
      return DART_ERR_INVAL;
    }
    /* descend to child at relative index: */
    domain     = domain->domains + subdomain_idx;
    /* continue scanning of domain tag at next part: */
    dot_begin  = dot_end;
    part_begin = dot_end+1;
    level++;
  }
  *locality = domain;
  DART_LOG_DEBUG("dart__base__locality__domain > team(%d) domain(%s) -> %p",
                 team, domain_tag, domain);
  return DART_OK;
}

/* ======================================================================== *
 * Unit Locality                                                            *
 * ======================================================================== */

dart_ret_t dart__base__locality__unit(
  dart_team_t             team,
  dart_unit_t             unit,
  dart_unit_locality_t ** locality)
{
  DART_LOG_DEBUG("dart__base__locality__unit() team(%d) unit(%d)",
                 team, unit);
  *locality = NULL;

  dart_unit_locality_t * uloc;
  dart_ret_t ret = dart__base__unit_locality__at(
                     dart__base__locality__unit_mapping_[team], unit, &uloc);
  if (ret != DART_OK) {
    DART_LOG_ERROR("dart_unit_locality: "
                   "dart__base__locality__unit(team:%d unit:%d) failed (%d)",
                   team, unit, ret);
    return ret;
  }
  *locality = uloc;

  DART_LOG_DEBUG("dart__base__locality__unit > team(%d) unit(%d)",
                 team, unit);
  return DART_OK;
}


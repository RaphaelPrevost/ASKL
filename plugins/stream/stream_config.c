/*******************************************************************************
 *  ASKL.                                                                      *
 *  Copyright (c) 2026 Raphael Prevost <raph@el.bzh>                           *
 *                                                                             *
 *  This software is a computer program whose purpose is to provide a          *
 *  framework for developing and prototyping network services.                 *
 *                                                                             *
 *  This software is governed by the CeCILL  license under French law and      *
 *  abiding by the rules of distribution of free software.  You can  use,      *
 *  modify and/ or redistribute the software under the terms of the CeCILL     *
 *  license as circulated by CEA, CNRS and INRIA at the following URL          *
 *  "http://www.cecill.info".                                                  *
 *                                                                             *
 *  As a counterpart to the access to the source code and  rights to copy,     *
 *  modify and redistribute granted by the license, users are provided only    *
 *  with a limited warranty  and the software's author,  the holder of the     *
 *  economic rights,  and the successive licensors  have only  limited         *
 *  liability.                                                                 *
 *                                                                             *
 *  In this respect, the user's attention is drawn to the risks associated     *
 *  with loading,  using,  modifying and/or developing or reproducing the      *
 *  software by the user in light of its specific status of free software,     *
 *  that may mean  that it is complicated to manipulate,  and  that  also      *
 *  therefore means  that it is reserved for developers  and  experienced      *
 *  professionals having in-depth computer knowledge. Users are therefore      *
 *  encouraged to load and test the software's suitability as regards their    *
 *  requirements in conditions enabling the security of their systems and/or   *
 *  data to be ensured and,  more generally, to use and operate it in the      *
 *  same conditions as regards security.                                       *
 *                                                                             *
 *  The fact that you are presently reading this means that you have had       *
 *  knowledge of the CeCILL license and that you accept its terms.             *
 *                                                                             *
 ******************************************************************************/

#include "stream.h"

static int personality = 0;

/* master options */
static unsigned int master_streams = 0;
static char *ingress_end[_STREAMS_MAX];
static char *workers_end[_STREAMS_MAX];

/* worker options */
static unsigned int worker_streams = 0;
static char *master_host[_STREAMS_MAX];
static char *master_port[_STREAMS_MAX];

static short destination[_STREAMS_MAX];

static char *server_host[_STREAMS_MAX];
static char *server_port[_STREAMS_MAX];
static char *module_name[_STREAMS_MAX];

/* -------------------------------------------------------------------------- */

INTERNAL int stream_config_init(int argc, char **argv)
{
    const char *opt_personality = NULL;
    const char *_master_streams = NULL;
    const char *opt_ingress_end = NULL;
    const char *opt_workers_end = NULL;
    const char *_worker_streams = NULL;
    const char *opt_master_host = NULL;
    const char *opt_master_port = NULL;
    const char *opt_destination = NULL;
    const char *opt_server_host = NULL;
    const char *opt_server_port = NULL;
    const char *opt_module_name = NULL;

    unsigned int i = 0;

    if (! (opt_personality = module_getopt("personality", argc, argv)) ) {
        fprintf(stderr, "Stream: missing option: \"personality\".\n");
        return -1;
    }

    if (! strcmp(opt_personality, "master"))
        personality = PERSONALITY_MASTER;
    else if (! strcmp(opt_personality, "worker"))
        personality = PERSONALITY_WORKER;
    else if (! strcmp(opt_personality, "hybrid"))
        personality = PERSONALITY_HYBRID;
    else {
        fprintf(
            stderr,
            "Stream: incorrect value for option: \"personality\".\n"
        );
        return -1;
    }

    if (personality & PERSONALITY_MASTER) {
        /* number of streams to configure */
        if (! (_master_streams = module_getopt("master_streams", argc, argv)) ) {
            /* default to single stream */
            master_streams = 1;
        } else master_streams = atoi(_master_streams);

        if (master_streams >= _STREAMS_MAX) {
            fprintf(
                stderr,
                "Stream: too many streams defined in the master configuration.\n"
            );
            fprintf(
                stderr,
                "Stream: a maximum of %i streams per instance can be defined.\n",
                _STREAMS_MAX
            );
            return -1;
        }

        for (i = 0; i < master_streams; i ++) {

            /* incoming traffic will flow through this port */
            opt_ingress_end = module_getarrayopt("ingress_end", i, argc, argv);
            if (! opt_ingress_end) {
                fprintf(stderr, "Stream: missing option: \"ingress_end\".\n");
                return -1;
            }

            /* TODO check ingress_end is numeric */

            ingress_end[i] = strndup(opt_ingress_end, strlen(opt_ingress_end));

            /* the workers will connect through this port */
            opt_workers_end = module_getarrayopt("workers_end", i, argc, argv);
            if (! opt_workers_end) {
                fprintf(stderr, "Stream: missing option: \"workers_end\".\n");
                return -1;
            }

            /* TODO check workers_end is numeric */

            workers_end[i] = strndup(opt_workers_end, strlen(opt_workers_end));
        }
    }

    if (personality & PERSONALITY_WORKER) {
        /* number of streams to configure */
        if (! (_worker_streams = module_getopt("worker_streams", argc, argv)) ) {
            /* default to single stream */
            worker_streams = 1;
        } else worker_streams = atoi(_worker_streams);

        /* the limit is shared for hybrid instances */
        if (worker_streams >= _STREAMS_MAX - master_streams) {
            fprintf(
                stderr,
                "Stream: too many streams defined in the worker configuration.\n"
            );
            fprintf(
                stderr,
                "Stream: a maximum of %i streams per instance can be defined.\n",
                _STREAMS_MAX
            );
            if (master_streams)
                fprintf(stderr, "Stream: %i master streams already configured.\n",
                        master_streams);
            return -1;
        }

        for (i = 0; i < worker_streams; i ++) {
            /* master's address */
            opt_master_host = module_getarrayopt("master_host", i, argc, argv);
            if (! opt_master_host) {
                fprintf(stderr, "Stream: missing option: \"master_host\".\n");
                return -1;
            }

            master_host[i] = strndup(opt_master_host, strlen(opt_master_host));

            opt_master_port = module_getarrayopt("master_port", i, argc, argv);
            if (! opt_master_port) {
                fprintf(stderr, "Stream: missing option: \"master_port\".\n");
                return -1;
            }

            master_port[i] = strndup(opt_master_port, strlen(opt_master_port));

            /* traffic can be forwarded to another service or handled by a module */
            opt_destination = module_getarrayopt("destination", i, argc, argv);
            if (! opt_destination) {
                fprintf(stderr, "Stream: missing option: \"destination\".\n");
                return -1;
            }

            if (! strcmp(opt_destination, "server")) {
                destination[i] = DESTINATION_SERVER;

                /* third party service host and port */
                opt_server_host = module_getarrayopt("server_host", i, argc, argv);
                if (! opt_server_host) {
                    fprintf(stderr, "Stream: missing option: \"server_host\".\n");
                    return -1;
                }

                server_host[i] = strndup(
                    opt_server_host,
                    strlen(opt_server_host)
                );

                opt_server_port = module_getarrayopt("server_port", i, argc, argv);
                if (! opt_server_port) {
                    fprintf(stderr, "Stream: missing option: \"server_port\".\n");
                    return -1;
                }

                server_port[i] = strndup(
                    opt_server_port,
                    strlen(opt_server_port)
                );
            } else if (! strcmp(opt_destination, "module")) {
                destination[i] = DESTINATION_MODULE;

                /* module name */
                opt_module_name = module_getarrayopt("module_name", i, argc, argv);
                if (! opt_module_name) {
                    fprintf(stderr, "Stream: missing option: \"module_name\".\n");
                    return -1;
                }

                module_name[i] = strndup(
                    opt_module_name,
                    strlen(opt_module_name)
                );
            } else {
                fprintf(
                    stderr,
                    "Stream: incorrect value for option: \"destination\".\n"
                );
                return -1;
            }
        }
    }

    return 0;
}

/* -------------------------------------------------------------------------- */

INTERNAL int stream_personality(void)
{
    return personality;
}

/* -------------------------------------------------------------------------- */

INTERNAL int stream_master_streams(void)
{
    return master_streams;
}

/* -------------------------------------------------------------------------- */

INTERNAL int stream_worker_streams(void)
{
    return worker_streams;
}

/* -------------------------------------------------------------------------- */

INTERNAL const char *stream_config_host(int stream, int target)
{
    if (stream < 0 || stream >= _STREAMS_MAX) {
        debug("stream_config_host(): bad parameters.\n");
        return NULL;
    }

    switch (target) {
    case MASTER_ADDRESS: return master_host[stream];
    case SERVER_ADDRESS: return server_host[stream];
    default: return NULL;
    }
}

/* -------------------------------------------------------------------------- */

INTERNAL const char *stream_config_port(int stream, int target)
{
    if (stream < 0 || stream >= _STREAMS_MAX) {
        debug("stream_config_port(): bad parameters.\n");
        return NULL;
    }

    switch (target) {
    case MASTER_ADDRESS: return master_port[stream];
    case SERVER_ADDRESS: return server_port[stream];
    case STREAM_INGRESS: return ingress_end[stream];
    case STREAM_WORKERS: return workers_end[stream];
    default: return NULL;
    }
}

/* -------------------------------------------------------------------------- */

INTERNAL int stream_config_destination(int stream)
{
    if (stream < 0 || stream >= _STREAMS_MAX) {
        debug("stream_config_destination(): bad parameters.\n");
        return -1;
    }

    return destination[stream];
}

/* -------------------------------------------------------------------------- */

INTERNAL const char *stream_config_module_name(int stream)
{
    if (stream < 0 || stream >= _STREAMS_MAX) {
        debug("stream_config_module_name(): bad parameters.\n");
        return NULL;
    }

    return module_name[stream];
}

/* -------------------------------------------------------------------------- */

INTERNAL void stream_config_exit(void)
{
    unsigned int i = 0;

    /* master */
    for (i = 0; i < master_streams; i ++) {
        free(ingress_end[i]);
        free(workers_end[i]);
    }

    /* worker */
    for (i = 0; i < worker_streams; i ++) {
        free(master_host[i]); free(master_port[i]);
        free(server_host[i]); free(server_port[i]);
        free(module_name[i]);
    }
}

/* -------------------------------------------------------------------------- */

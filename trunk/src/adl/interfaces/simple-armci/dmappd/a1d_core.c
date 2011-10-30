/********************************************************************
 * The following is a notice of limited availability of the code, and disclaimer
 * which must be included in the prologue of the code and in all source listings
 * of the code.
 *
 * Copyright (c) 2010 Argonne Leadership Computing Facility, Argonne National Laboratory
 *
 * Permission is hereby granted to use, reproduce, prepare derivative works, and
 * to redistribute to others.
 *
 *                 LICENSE
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 *  - Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer listed
 *    in this license in the documentation and/or other materials
 *    provided with the distribution.
 *
 *  - Neither the name of the copyright holders nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * The copyright holders provide no reassurances that the source code
 * provided does not infringe any patent, copyright, or any other
 * intellectual property rights of third parties.  The copyright holders
 * disclaim any liability to any recipient for claims brought against
 * recipient by any third party for infringement of that parties
 * intellectual property rights.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *********************************************************************/

#include "a1d_core.h"
#include "a1d_comm.h"
#include "a1d_atomic.h"
#include "a1d_stats.h"

int pmi_rank;
int pmi_size;


int A1D_Rank()
{
    return pmi_rank;
}

int A1D_Size()
{
    return pmi_size;
}

int A1D_Initialize()
{
    int pmi_spawned = 0;

#ifdef __CRAYXE
    int                   pmi_status  = PMI_SUCCESS;
    dmapp_return_t        dmapp_status = DMAPP_RC_SUCCESS;

    dmapp_rma_attrs_ext_t dmapp_config_in, dmapp_config_out;

    dmapp_jobinfo_t       dmapp_info;
    dmapp_pe_t            dmapp_rank = -1;
    int                   dmapp_size = -1;

    uint64_t              world_pset_concat_buf_size = -1;
    void *                world_pset_concat_buf = NULL;

    dmapp_c_pset_desc_t   dmapp_world_desc;
    uint64_t              dmapp_world_id = 1000;
    uint64_t              dmapp_world_modes = DMAPP_C_PSET_MODE_CONCAT; /* TODO: do I need this bit set? */

    uint32_t              dmapp_reduce_max_int32t = 0;
    uint32_t              dmapp_reduce_max_int64t = 0;
#endif

#ifdef DEBUG_FUNCTION_ENTER_EXIT
    fprintf(stderr,"entering A1D_Initialize() \n");
#endif

#ifdef __CRAYXE

    /***************************************************
     *
     * configure PMI
     *
     ***************************************************/

    /* initialize PMI (may not be necessary */
    pmi_status = PMI_Init(&pmi_spawned));
    assert(pmi_status==PMI_SUCCESS);
    if (pmi_spawned==PMI_TRUE)
        fprintf(stderr,"PMI says this process is spawned.  This is bad. \n");
    assert(pmi_spawned==PMI_FALSE);

    /* get my PMI rank */
    pmi_status = PMI_Get_rank(&pmi_rank);
    assert(pmi_status==PMI_SUCCESS);

    /* get PMI world size */
    pmi_status = PMI_Get_size(&pmi_size);
    assert(pmi_status==PMI_SUCCESS);

    /***************************************************
     *
     * configure DMAPP
     *
     ***************************************************/

    dmapp_config_in.max_outstanding_nb   = DMAPP_DEF_OUTSTANDING_NB;
    dmapp_config_in.offload_threshold    = DMAPP_OFFLOAD_THRESHOLD;
    dmapp_config_in.put_relaxed_ordering = DMAPP_ROUTING_DETERMINISTIC;
    dmapp_config_in.get_relaxed_ordering = DMAPP_ROUTING_DETERMINISTIC;
    dmapp_config_in.max_concurrency      = 1; /* not thread-safe */
#ifdef FLUSH_IMPLEMENTED
    dmapp_config_in.PI_ordering          = DMAPP_PI_ORDERING_RELAXED;
#else
    dmapp_config_in.PI_ordering          = DMAPP_PI_ORDERING_STRICT;
#endif

    dmapp_status = dmapp_init_ext( &dmapp_config_in, &dmapp_config_out );
    assert(dmapp_status==DMAPP_RC_SUCCESS);

#ifndef FLUSH_IMPLEMENTED
    /* without strict PI ordering, we have to flush remote stores with a get packet to force global visibility */
    assert( dmapp_config_out.PI_ordering != DMAPP_PI_ORDERING_STRICT);
#endif

    dmapp_status = dmapp_get_jobinfo(&dmapp_info);
    assert(dmapp_status==DMAPP_RC_SUCCESS);

    dmapp_rank = dmapp_info.pe;
    dmapp_size = dmapp_info.npes;
    memcpy( A1D_Sheap_desc, dmapp_info.sheap_seg, sizeof(dmapp_seg_desc_t) ); /* TODO: better to keep pointer instead? */

    /* make sure PMI and DMAPP agree */
    assert(pmi_rank==dmapp_rank);
    assert(pmi_size==dmapp_size);

    /* necessary? */
    pmi_status = PMI_Barrier();
    assert(pmi_status==PMI_SUCCESS);

    /***************************************************
     *
     * setup DMAPP world pset
     *
     ***************************************************/

    dmapp_result = dmapp_c_greduce_nelems_max(DMAPP_C_INT32, &dmapp_reduce_max_int32t);
    dmapp_result = dmapp_c_greduce_nelems_max(DMAPP_C_INT64, &dmapp_reduce_max_int64t);

    /* allocate proportional to job size, since this is important for performance of concatenation */
    world_pset_concat_buf_size = 8 * pmi_size;
    world_pset_concat_buf = dmapp_sheap_malloc( world_pset_concat_buf_size );

    dmapp_world_desc.concat_buf                    = world_pset_concat_buf;
    dmapp_world_desc.concat_bufsize                = world_pset_concat_buf_size;
    dmapp_world_desc.dmapp_c_pset_delimiter_type_t = DMAPP_C_PSET_DELIMITER_STRIDED; /* FYI: this is only documented in dmapp.h */
#ifdef A1D_C99_STRUCT_INIT
    dmapp_world_desc.u.stride_type                 = { .n_pes = pmi_size, .base_pe = 0, .stride_pe = 1 };
#else
    dmapp_world_desc.u.stride_type                 = { pmi_size, 0, 1 };
#endif

    dmapp_status = dmapp_c_pset_create( &dmapp_world_desc, dmapp_world_id, dmapp_world_modes, NULL, A1D_Pset_world );
    assert(dmapp_status==DMAPP_RC_SUCCESS);

    /* out-of-band sync required between pset create and pset export */
    pmi_status = PMI_Barrier();
    assert(pmi_status==PMI_SUCCESS);

    /* export pset after out-of-band sync */
    dmapp_status = dmapp_c_pset_export( &A1D_Pset_world );
    assert(dmapp_status==DMAPP_RC_SUCCESS);

#endif

    /***************************************************
     *
     * setup protocols
     *
     ***************************************************/

    A1DI_Atomic_Initialize();

    A1DI_Get_Initialize();

    A1DI_Put_Initialize();
#  ifdef FLUSH_IMPLEMENTED
    /* allocate Put list */
    A1D_Put_flush_list = malloc( pmi_size * sizeof(int) );
    assert(A1D_Put_flush_list != NULL);
#  endif

    A1DI_Acc_Initialize();

#ifdef DEBUG_FUNCTION_ENTER_EXIT
    fprintf(stderr,"exiting A1D_Initialize() \n");
#endif

    return(0);
}

int A1D_Finalize()
{
    int pmi_status;
    int i;

#ifdef DEBUG_FUNCTION_ENTER_EXIT
    fprintf(stderr,"entering A1D_Finalize() \n");
#endif

    A1D_Print_stats();

    /* barrier so that no one is able to access remote memregions after they are destroyed */
    pmi_status = PMI_Barrier();
    assert(pmi_status==PMI_SUCCESS);

#ifdef DEBUG_FUNCTION_ENTER_EXIT
    fprintf(stderr,"exiting A1D_Finalize() \n");
#endif

    return(0);
}

/***************************************************
 *
 * local memory allocation
 *
 ***************************************************/

void * A1D_Allocate_local(int bytes)
{
    void * tmp;

#ifdef DEBUG_FUNCTION_ENTER_EXIT
    fprintf(stderr,"entering A1D_Allocate_local(void ** ptr, int bytes) \n");
#endif

    if (bytes>0) 
    {
        tmp = calloc(bytes,1);
        assert( tmp != NULL );
    }
    else
    {
        if (bytes<0)
        {
            fprintf(stderr, "You requested %d bytes.  What kind of computer do you think I am? \n",bytes);
            fflush(stderr);
        }
        tmp = NULL;
    }

#ifdef DEBUG_FUNCTION_ENTER_EXIT
    fprintf(stderr,"exiting A1D_Allocate_local(void ** ptr, int bytes) \n");
#endif

    return tmp;
}

void A1D_Free_local(void * ptr)
{
#ifdef DEBUG_FUNCTION_ENTER_EXIT
    fprintf(stderr,"entering A1D_Free_local(void* ptr) \n");
#endif

    if (ptr != NULL)
    {
        free(ptr);
    }
    else
    {
        fprintf(stderr, "You tried to free a NULL pointer.  Please check your code. \n");
        fflush(stderr);
    }

#ifdef DEBUG_FUNCTION_ENTER_EXIT
    fprintf(stderr,"exiting A1D_Free_local(void* ptr) \n");
#endif

    return;
}

/***************************************************
 *
 * global shared memory allocation
 *
 ***************************************************/

int A1D_Allocate_shared(void * ptrs[], int bytes)
{
    int i = 0;
#ifdef __CRAYXE
    int            pmi_status   = PMI_SUCCESS;
    dmapp_return_t dmapp_status = DMAPP_RC_SUCCESS;
    dmapp_pe_t *   pe_list      = NULL;
#endif
    void *  tmp_ptr       = NULL;
    int     max_bytes_in  = 0;
    int     max_bytes_out = 0;

#ifdef DEBUG_FUNCTION_ENTER_EXIT
    fprintf(stderr,"entering A1D_Allocate_shared(void* ptrs[], int bytes) \n");
#endif

#ifdef __CRAYXE

    /* barrier so that no one tries to access memory which is no longer allocated */
    pmi_status = PMI_Barrier();
    assert(pmi_status==0);

    /* preserve symmetric heap condition */
    max_bytes_in = bytes;
    dmapp_status = dmapp_c_greduce_start( &A1D_Pset_world, max_bytes_in, max_bytes_out, 1, DMAPP_C_INT32, DMAPP_C_MAX );

    /* wait for greduce to finish */
    dmapp_status = dmapp_c_pset_wait( A1D_Pset_world );

    /* barrier because greduce semantics are not clear */
    pmi_status = PMI_Barrier();
    assert(pmi_status==0);

    /* finally allocate memory from symmetric heap */
    tmp_ptr = dmapp_sheap_malloc( (size_t)max_bytes_out );
    assert(dmapp_status==DMAPP_RC_SUCCESS);

    /* barrier again for good measure */
    pmi_status = PMI_Barrier();
    assert(pmi_status==0);

    /* allgather addresses into pointer vector */
    pe_list = (dmapp_pe_t *) malloc( pmi_size * sizeof(dmapp_pe_t) );
    for ( i=0; i<pmi_size; i++) pe_list[i] = i;
    dmapp_status = dmapp_gather_ixpe( &tmp_ptr, ptrs, &A1D_Sheap_desc, pe_list, pmi_size, 1, DMAPP_QW );

    /* barrier again for good measure */
    pmi_status = PMI_Barrier();
    assert(pmi_status==0);

    /* cleanup from allgather */
    free(pe_list);

#endif

#ifdef DEBUG_FUNCTION_ENTER_EXIT
    fprintf(stderr,"exiting A1D_Allocate_shared(void* ptrs[], int bytes) \n");
#endif

    return(0);
}


void A1D_Free_shared(void * ptr)
{
#ifdef __CRAYXE
    int pmi_status = PMI_SUCCESS;
    dmapp_return_t dmapp_status = DMAPP_RC_SUCCESS;
#endif

#ifdef DEBUG_FUNCTION_ENTER_EXIT
    fprintf(stderr,"entering A1D_Free_shared(void* ptr) \n");
#endif

#ifdef __CRAYXE
    /* barrier so that no one tries to access memory which is no longer allocated */
    pmi_status = PMI_Barrier();
    assert(pmi_status==0);

    dmapp_sheap
    assert(dmapp_status==DMAPP_RC_SUCCESS);
#endif

#ifdef DEBUG_FUNCTION_ENTER_EXIT
    fprintf(stderr,"exiting A1D_Free_shared(void* ptr) \n");
#endif

    return;
}
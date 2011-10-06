/* 
 * The following is a notice of limited availability of the code, and disclaimer
 * which must be included in the prologue of the code and in all source listings
 * of the code.
 *
 * Copyright (c) 2010  Argonne Leadership Computing Facility, Argonne National
 * Laboratory
 *
 * Permission is hereby granted to use, reproduce, prepare derivative works, and
 * to redistribute to others.
 *
 *
 *                          LICENSE
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer listed
 *   in this license in the documentation and/or other materials
 *   provided with the distribution.
 *
 * - Neither the name of the copyright holders nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
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
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <mpi.h>
#include "parmci.h"

int main(int argc, char *argv[])
{
    int rank, size;
    int provided;

    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    PARMCI_Init_args(&argc, &argv);

    int w = ( argc > 1 ? atoi(argv[1]) : 10 );

    if ( rank == 0 ) printf( "size = %d w = %d doubles\n", size, w );

    int ** window;
    window  = (int **) PARMCI_Malloc_local( size * sizeof(int *) );
    PARMCI_Malloc( (void **) window, w * sizeof(int) );
    for (int i = 0; i < w; i++) window[rank][i] = 0;

    double * buffer;
    buffer = (double *) PARMCI_Malloc_local(  w * sizeof(double) );

    PARMCI_Barrier();

    if (rank == 0)
        for (int t=1; t<size; t++)
        {
            int bytes = w * sizeof(int);

            for (int i = 0; i < w; i++) buffer[i] = t;

            PARMCI_Put( buffer, window[t], bytes, t );
            PARMCI_Fence( t );

            for (int i = 0; i < w; i++) buffer[i] = 0;

            PARMCI_Get( window[t], buffer, bytes, t );

            int errors = 0;

            for (int i = 0; i < w; i++) 
                if ( buffer[i] != t ) errors++;

            if ( errors > 0 )
                for (int i = 0; i < w; i++) 
                    printf("rank %d buffer[%d] = %d \n", rank, i, buffer[i] );
        }

    PARMCI_Barrier();

    if (rank != 0)
    {
       int errors = 0;

       for (int i = 0; i < w; i++) 
           if ( window[rank][i] != rank ) errors++;

       if ( errors > 0 )
           for (int i = 0; i < w; i++) 
               printf("rank %d window[%d][%d] = %d \n", rank, rank, i, window[rank][i] );
    }

    PARMCI_Barrier();

    if (rank == 0)
        for (int t=1; t<size; t++)
        {
            int bytes = w * sizeof(int);

            double t0, t1, t2, dt1, dt2, bw1, bw2;

            for (int i = 0; i < w; i++) buffer[i] = -1;

            t0 = MPI_Wtime();
            PARMCI_Put( buffer, window[t], bytes, t );
            t1 = MPI_Wtime();
            PARMCI_Fence( t );
            t2 = MPI_Wtime();

            dt1 =  ( t1 - t0 );
            dt2 =  ( t2 - t0 );
            bw1 = (double)bytes / dt1 / 1000000;
            bw2 = (double)bytes / dt2 / 1000000;
            printf("PARMCI_Put of from rank %d to rank %d of %d bytes - local: %lf s (%lf MB/s) remote: %lf s (%lf MB/s) \n",
                   t, 0, bytes, dt1, bw1, dt2, bw2);
            fflush(stdout);
        }

    PARMCI_Barrier();

    PARMCI_Free_local( (void *) buffer );

    PARMCI_Free( (void *) window[rank] );
    PARMCI_Free_local( (void *) window );

    PARMCI_Finalize();

    MPI_Finalize();

    return 0;
}
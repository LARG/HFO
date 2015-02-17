// -*-c++-*-

/*
 *Copyright:

 Copyright (C) Hidehisa AKIYAMA

 This code is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 3, or (at your option)
 any later version.

 This code is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this code; see the file COPYING.  If not, write to
 the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

 *EndCopyright:
 */

/////////////////////////////////////////////////////////////////////

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "sample_trainer.h"

#include <rcsc/common/basic_client.h>

#include <iostream>
#include <cstdlib> // exit
#include <cerrno> // errno
#include <cstring> // strerror
#include <csignal> // sigaction

namespace {

SampleTrainer agent;

/*-------------------------------------------------------------------*/
void
sig_exit_handle( int )
{
    std::cerr << "Killed. Exiting trainer..." << std::endl;
    agent.finalize();
    std::exit( EXIT_FAILURE );
}

}


/*-------------------------------------------------------------------*/
int
main( int argc, char ** argv )
{
    struct sigaction sig_action ;
    sig_action.sa_handler = &sig_exit_handle ;
    sig_action.sa_flags = 0;
    if ( sigaction( SIGINT, &sig_action , NULL ) != 0
         || sigaction( SIGTERM, &sig_action , NULL ) != 0
         || sigaction( SIGHUP, &sig_action , NULL ) != 0 )
        /*if ( signal(SIGINT, &sigExitHandle) == SIG_ERR
          || signal(SIGTERM, &sigExitHandle) == SIG_ERR
          || signal(SIGHUP, &sigExitHandle) == SIG_ERR )*/
    {
        std::cerr << __FILE__ << ": " << __LINE__
                  << ": could not set signal handler: "
                  << std::strerror( errno ) << std::endl;
        std::exit( EXIT_FAILURE );
    }

    rcsc::BasicClient client;

    if ( ! agent.init( &client, argc, argv ) )
    {
        return EXIT_FAILURE;
    }

    /*
      You should add your copyright message here.
     */
    // std::cout << "*****************************************************************\n"
    //           << " This program is modified by <Team Name>\n"
    //           << " Copyright 20xx. <Your name>.\n"
    //           << " <Affiliation>\n"
    //           << " All rights reserved.\n"
    //           << "*****************************************************************\n"
    //           << std::flush;

    /*
      Do NOT remove the following copyright notice!
     */
    std::cout << "*****************************************************************\n"
              << " This program is based on agent2d created by Hidehisa Akiyama.\n"
              << " Copyright 2006 - 2010. Hidehisa Akiyama.\n"
              << " National Institute of Advanced Industrial Science and Technology\n"
              << " All rights reserved.\n"
              << "*****************************************************************\n"
              << std::flush;

    client.run( &agent );

    return EXIT_SUCCESS;
}

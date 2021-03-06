/*
 * --- License -------------------------------------------------------------- *
 */

/*
 * Copyright 2017 NIKI 4.0 project team
 *
 * NIKI 4.0 was financed by the Baden-Württemberg Stiftung gGmbH (www.bwstiftung.de).
 * Project partners are FZI Forschungszentrum Informatik am Karlsruher
 * Institut für Technologie (www.fzi.de), Hahn-Schickard-Gesellschaft
 * für angewandte Forschung e.V. (www.hahn-schickard.de) and
 * Hochschule Offenburg (www.hs-offenburg.de).
 * This file was developed by the Institute of reliable Embedded Systems
 * and Communication Electronics
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

/*
 * --- Module Description --------------------------------------------------- *
 */

/**
 * \file    LWM2MServer.Cpp
 * \author  Institute of reliable Embedded Systems
 *          and Communication Electronics
 * \date    $Date$
 * \version $Version$
 *
 * \brief   Description of a LWM2M Server
 *
 */

/*
 * --- Includes ------------------------------------------------------------- *
 */
#include <stdint.h>
#include <string.h>
#include <iostream>
#include <string>
#include "LWM2MServer.h"
#include "LWM2MDevice.h"
#include "LWM2MObject.h"
#include "LWM2MResource.h"
#include "er-coap-13/er-coap-13.h"


/*
 * --- Macro Definitions----------------------------------------------------- *
 */

/** Maximum size of a packet */
#define LWM2MSERVER_MAX_PACKET_SIZE         1500

#ifdef OPCUA_LWM2M_SERVER_USE_THREAD
#define OPCUA_LWM2M_SERVER_MUTEX_LOCK(a)        pthread_mutex_lock( &(a)->m_mutex );
#define OPCUA_LWM2M_SERVER_MUTEX_UNLOCK(a)      pthread_mutex_unlock( &(a)->m_mutex );
#define OPCUA_LWM2M_SERVER_SLEEP(a)
#else
#define OPCUA_LWM2M_SERVER_MUTEX_LOCK(a)
#define OPCUA_LWM2M_SERVER_MUTEX_UNLOCK(a)
#define OPCUA_LWM2M_SERVER_SLEEP(a)             usleep(a)
#endif /* #ifdef OPCUA_LWM2M_SERVER_USE_THREAD */


/** Sleeptime while running the OPC UA Server */
#define LWM2MSERVER_RUN_TOT_US                  5000

/*
 * --- Methods Definition --------------------------------------------------- *
 */


/*---------------------------------------------------------------------------*/
/*
* LWM2MServer::~LWM2MServer()
*/
LWM2MServer::~LWM2MServer( void )
{
    /* stop running server */
    stopServer();

    /* delete devices */
    std::map< std::string, LWM2MDevice* >::iterator it = m_devMap.begin();

    /*delete all Objects */
    while( it != m_devMap.end() )
    {
        delete (it->second);
        it++;
    }

} /* LWM2MServer::~LWM2MServer() */


/*---------------------------------------------------------------------------*/
/*
* LWM2MServer::startServer()
*/
int16_t LWM2MServer::startServer( void )
{
    int16_t ret = 0;

    OPCUA_LWM2M_SERVER_MUTEX_LOCK(this);

    /* stop running server before restarting */
    if( stopServer() != 0)
        ret = -1;

    if( ret == 0 )
    {
        /* create the socket */
        m_sock = create_socket( m_port.c_str(), m_addrFam );
        if( m_sock < 0 )
        {
            /* socket could not be created */
            ret = -2;
        }
    }

    if( ret == 0 )
    {
        /* initialize LWM2M context */
        mp_lwm2mH = lwm2m_init( NULL );
        if (NULL == mp_lwm2mH)
            /* LWM2M context could not be created */
            ret = -3;

        /* register monitoring callback */
        lwm2m_set_monitoring_callback( mp_lwm2mH, monitorCb, this );
    }

    OPCUA_LWM2M_SERVER_MUTEX_UNLOCK(this);

#ifdef OPCUA_LWM2M_SERVER_USE_THREAD
    if( ret == 0 )
    {
        /* start the Thread */
        m_threadRun = true;
        pthread_create( &m_thread, NULL, threadEntryFunc, this );
    }
#endif /* #ifdef OPCUA_LWM2M_SERVER_USE_THREAD */

    return ret;

} /* LWM2MServer::startServer() */

/*---------------------------------------------------------------------------*/
/*
* LWM2MServer::stopServer()
*/
int16_t LWM2MServer::stopServer( void )
{

#ifdef OPCUA_LWM2M_SERVER_USE_THREAD
    /* stop the server thread and wait */
    m_threadRun = false;

    if( m_thread != -0 )
    {
        pthread_join(m_thread, NULL);
        m_thread = 0;
    }
#endif /* #ifdef OPCUA_LWM2M_SERVER_USE_THREAD */

    OPCUA_LWM2M_SERVER_MUTEX_LOCK(this);

    if( mp_connList != NULL )
    {
        /* reset the connection list */
        connection_free( mp_connList );
        mp_connList = NULL;
    }

    if( m_sock != -1 )
    {
        /* socket was used and must be closed */
        close( m_sock );
        m_sock = -1;
    }

    if( mp_lwm2mH != NULL )
    {
        /* close existing LWM2M context */
        lwm2m_close( mp_lwm2mH );
        mp_lwm2mH = NULL;
    }

    OPCUA_LWM2M_SERVER_MUTEX_UNLOCK(this);

    return 0;

} /* LWM2MServer::stopServer() */

/*---------------------------------------------------------------------------*/
/*
* LWM2MServer::runServer()
*/
int16_t LWM2MServer::runServer( void )
{
    int16_t ret = 0;
    fd_set readfds;
    struct timeval tv;
    int result;

    FD_ZERO( &readfds );
    FD_SET( m_sock, &readfds );

    tv.tv_sec = 0;
    tv.tv_usec = 100000;

    OPCUA_LWM2M_SERVER_MUTEX_LOCK(this);

#ifdef OPCUA_LWM2M_SERVER_USE_THREAD
    if( m_threadRun == false )
        ret = -1;
#endif /* #ifdef OPCUA_LWM2M_SERVER_USE_THREAD */

    /* check for pending events */
    checkEvents();

    /* Check for deleted devices */
    checkDeletedDevices();

    if( ret == 0 )
    {
        result = lwm2m_step(mp_lwm2mH, &(tv.tv_sec) );
        if (result != 0)
            ret = -1;
    }

    OPCUA_LWM2M_SERVER_MUTEX_UNLOCK(this);

    if( ret == 0 )
    {
        result = select(FD_SETSIZE, &readfds, 0, 0, &tv);
        if ( result < 0 )
            ret = -1;
    }

    OPCUA_LWM2M_SERVER_MUTEX_LOCK(this);
    if( ret == 0 )
    {
        if (result > 0)
        {
            uint8_t buffer[LWM2MSERVER_MAX_PACKET_SIZE];
            int numBytes;

            if( FD_ISSET( m_sock, &readfds ) )
            {
                struct sockaddr_storage addr;
                socklen_t addrLen;

                addrLen = sizeof(addr);
                numBytes = recvfrom( m_sock, buffer, LWM2MSERVER_MAX_PACKET_SIZE, MSG_DONTWAIT,
                        (struct sockaddr *)&addr, &addrLen);

                if( (ret == 0) && (numBytes > 0) )
                {
                    char s[INET6_ADDRSTRLEN];
                    connection_t * connP;

                    s[0] = 0;
                    if (AF_INET == addr.ss_family)
                    {
                        struct sockaddr_in *saddr = (struct sockaddr_in *)&addr;
                        inet_ntop(saddr->sin_family, &saddr->sin_addr, s, INET6_ADDRSTRLEN);
                    }
                    else if (AF_INET6 == addr.ss_family)
                    {
                        struct sockaddr_in6 *saddr = (struct sockaddr_in6 *)&addr;
                        inet_ntop(saddr->sin6_family, &saddr->sin6_addr, s, INET6_ADDRSTRLEN);
                    }

                    connP = connection_find(mp_connList, &addr, addrLen);
                    if( connP == NULL )
                    {
                        connP = connection_new_incoming( mp_connList, m_sock,
                                (struct sockaddr *)&addr, addrLen );
                        if( connP != NULL )
                            mp_connList = connP;
                    }
                    if( connP != NULL )
                        lwm2m_handle_packet( mp_lwm2mH, buffer, numBytes, connP );
                }
            }
        }
    }
    OPCUA_LWM2M_SERVER_MUTEX_UNLOCK(this);

    return ret;
} /* LWM2MServer::runServer() */


/*---------------------------------------------------------------------------*/
/*
* LWM2MServer::hasDevice
*/
bool LWM2MServer::hasDevice( std::string client )
{
    bool ret = true;
    OPCUA_LWM2M_SERVER_MUTEX_LOCK(this);

    if( !isAlive() )
        ret = false;

    if( ret == true )
    {
        if( getDevice( client ) == NULL )
            ret = false;
        else
            ret = true;
    }

    OPCUA_LWM2M_SERVER_MUTEX_UNLOCK(this);
    return ret;

} /* LWM2MServer::hasDevice() */


/*---------------------------------------------------------------------------*/
/*
* LWM2MServer::getLWM2MDevice
*/
LWM2MDevice* LWM2MServer::getLWM2MDevice( std::string client )
{
    LWM2MDevice* ret = NULL;
    std::map< std::string, LWM2MDevice* >::iterator it;

    OPCUA_LWM2M_SERVER_MUTEX_LOCK(this);
    it = m_devMap.begin();
    while( it != m_devMap.end() )
    {
      if( it->first == client )
        break;
      it++;
    }

    if( it != m_devMap.end() )
      ret = it->second;
    OPCUA_LWM2M_SERVER_MUTEX_UNLOCK(this);

    return ret;

} /* LWM2MServer::getLWM2MDevice() */


/*---------------------------------------------------------------------------*/
/*
* LWM2MServer::read()
*/
int8_t LWM2MServer::read( const LWM2MResource* p_res, lwm2m_data_t** val,
    s_lwm2m_obsparams_t* p_cbParams )
{
    int8_t ret = 0;

    lwm2m_client_t* p_cli;
    const LWM2MDevice* p_dev;
    const LWM2MObject* p_obj;
    lwm2m_uri_t uri;

    s_lwm2m_obsparams_t* p_cbData = NULL;
    s_lwm2m_obsparams_t cbData;
    int lwm2mRet;

    if( p_cbParams == NULL )
    {
        /* create local cb parameters for blocking operation */
        p_cbData = &cbData;
        memset( p_cbData, 0, sizeof(s_lwm2m_obsparams_t) );
    }
    else
    {
        /* non-blocking operation with callback parameters */
        p_cbData = p_cbParams;
    }

    OPCUA_LWM2M_SERVER_MUTEX_LOCK(this);

    if( (!isAlive()) || (p_res == NULL) )
        ret = -1;

    if( ret == 0 )
    {
        /* get object of the resource */
        p_obj = p_res->getObject();
        if( p_obj == NULL )
            ret = -1;
    }

    if( ret == 0 )
    {
        /* get device of the resource */
        p_dev = p_obj->getDevice();
        if( p_dev == NULL )
            ret = -1;
    }

    if( ret == 0 )
    {
        /* find the device in the list of registered devices */
        p_cli = getDevice( p_dev->getName() );
        if( p_cli == NULL )
            ret = -1;
    }

    if( ret == 0 )
    {
        /* start the query with the according values */
        uri.objectId = p_obj->getObjId();
        uri.instanceId = p_obj->getInstId();
        uri.resourceId = p_res->getResId();
        uri.flag = LWM2M_URI_FLAG_OBJECT_ID | LWM2M_URI_FLAG_INSTANCE_ID |
                LWM2M_URI_FLAG_RESOURCE_ID;

        p_cbData->status = NO_ERROR;
        lwm2mRet = lwm2m_dm_read( mp_lwm2mH, p_cli->internalID, &uri, readWriteResCb, p_cbData);

        if( lwm2mRet != COAP_NO_ERROR )
            ret = -1;
    }

    OPCUA_LWM2M_SERVER_MUTEX_UNLOCK(this);

    if( ret == 0 )
    {
        if( p_cbParams == NULL )
        {
            while( true )
            {
                int status;
                OPCUA_LWM2M_SERVER_MUTEX_LOCK(this);
#ifndef OPCUA_LWM2M_SERVER_USE_THREAD
                /* call the server */
                runServer();
#endif /* #ifndef OPCUA_LWM2M_SERVER_USE_THREAD */
                status = p_cbData->status;;
                OPCUA_LWM2M_SERVER_MUTEX_UNLOCK(this);
                if( status != NO_ERROR )
                    break;
                OPCUA_LWM2M_SERVER_SLEEP(LWM2MSERVER_RUN_TOT_US);
            }

            if( p_cbData->status == CONTENT_2_05)
            {
                *val = p_cbData->data;
                ret = p_cbData->dataLen;
            }
            else
                ret = -1;
        }
        else
        {
            /* do not wait for the result */
            ret = 0;
        }
    }
    return ret;

} /* LWM2MServer::read() */


/*---------------------------------------------------------------------------*/
/*
* LWM2MServer::write()
*/
int8_t LWM2MServer::write( const LWM2MResource* p_res, const std::string& val,
    s_lwm2m_obsparams_t* p_cbParams )
{
    int8_t ret = 0;
    lwm2m_client_t* p_cli;
    const LWM2MDevice* p_dev;
    const LWM2MObject* p_obj;
    lwm2m_uri_t uri;
    s_lwm2m_obsparams_t* p_cbData = NULL;
    s_lwm2m_obsparams_t cbData;
    int lwm2mRet;

    if( p_cbParams == NULL )
    {
        /* create local cb parameters for blocking operation */
        p_cbData = &cbData;
        memset( p_cbData, 0, sizeof(s_lwm2m_obsparams_t) );
    }
    else
    {
        /* non-blocking operation with callback parameters */
        p_cbData = p_cbParams;
    }

    OPCUA_LWM2M_SERVER_MUTEX_LOCK(this);

    if( (!isAlive()) || (p_res == NULL)   )
        ret = -1;

    /* get object of the resource */
    p_obj = p_res->getObject();
    if( p_obj == NULL )
        ret = -1;

    if( ret == 0 )
    {
        /* get device of the resource */
        p_dev = p_obj->getDevice();
        if( p_dev == NULL )
            ret = -1;
    }

    if( ret == 0 )
    {
        /* find the device in the list of registered devices */
        p_cli = getDevice( p_dev->getName() );
        if( p_cli == NULL )
            ret = -1;
    }

    if( ret == 0 )
    {
        /* start the query with the according values */
        uri.objectId = p_obj->getObjId();
        uri.instanceId = p_obj->getInstId();
        uri.resourceId = p_res->getResId();
        uri.flag = LWM2M_URI_FLAG_OBJECT_ID | LWM2M_URI_FLAG_INSTANCE_ID | LWM2M_URI_FLAG_RESOURCE_ID;

        p_cbData->status = NO_ERROR;

        lwm2mRet = lwm2m_dm_write( mp_lwm2mH, p_cli->internalID, &uri, LWM2M_CONTENT_TEXT,
                (uint8_t*)val.c_str(), val.length(), readWriteResCb, p_cbData  );


        if( lwm2mRet != COAP_NO_ERROR )
                ret = -1;
    }

    OPCUA_LWM2M_SERVER_MUTEX_UNLOCK(this);

    if( ret == 0 )
    {
        if( p_cbParams == NULL )
        {
            while( true )
            {
                int status;
                OPCUA_LWM2M_SERVER_MUTEX_LOCK(this);
#ifndef OPCUA_LWM2M_SERVER_USE_THREAD
                /* call the server */
                runServer();
#endif /* #ifndef OPCUA_LWM2M_SERVER_USE_THREAD */
                status = p_cbData->status;
                OPCUA_LWM2M_SERVER_MUTEX_UNLOCK(this);
                if( status != NO_ERROR )
                    break;
                OPCUA_LWM2M_SERVER_SLEEP(LWM2MSERVER_RUN_TOT_US);
            }
        }
        else
        {
            /* do not wait for the result */
            ret = 0;
        }
    }

    if( p_cbData->status == CHANGED_2_04 )
        ret = 0;

    return ret;

} /* LWM2MServer::write() */



/*---------------------------------------------------------------------------*/
/*
* LWM2MServer::observe()
*/
int8_t LWM2MServer::observe( const LWM2MObject* p_obj, bool observe )
{
    int8_t ret = 0;
    lwm2m_client_t* p_cli;
    const LWM2MDevice* p_dev;
    lwm2m_uri_t uri;

    std::map< const LWM2MObject*, s_lwm2m_obsparams_t*>::iterator it;
    s_lwm2m_obsparams_t* p_cbData = NULL;
    int lwm2mRet;

    if( p_obj == NULL )
        /* Invalid arguments */
        ret = -1;

    if( ret == 0 )
    {
        it = m_obsObjMap.find( p_obj );

        /* get the observe parameters */
        if( it == m_obsObjMap.end() )
        {
            if( observe == true )
            {
                /*create a new entry for the resource */
                p_cbData = new s_lwm2m_obsparams_t();

                m_obsObjMap.insert(std::pair< const LWM2MObject*, s_lwm2m_obsparams_t* >
                    ( p_obj, p_cbData ) );
            }
        }
        else
        {
            /* use existing parameter */
            p_cbData = it->second;
        }
    }

    OPCUA_LWM2M_SERVER_MUTEX_LOCK(this);

    if( ret == 0 )
    {
        if( (!isAlive()) || (p_obj == NULL) )
            ret = -1;
    }

    if( ret == 0 )
    {
        /* get device of the resource */
        p_dev = p_obj->getDevice();
        if( p_dev == NULL )
            ret = -1;
    }

    if( ret == 0 )
    {
        /* find the device in the list of registered devices */
        p_cli = getDevice( p_dev->getName() );
        if( p_cli == NULL )
            ret = -1;
    }

    if( ret == 0 )
    {
        /* start the query with the according values */
        uri.objectId = p_obj->getObjId();
        uri.instanceId = p_obj->getInstId();
        uri.flag = LWM2M_URI_FLAG_OBJECT_ID | LWM2M_URI_FLAG_INSTANCE_ID;

        if( observe == true )
        {
            /* start observation */
            p_cbData->status = -1;

            lwm2mRet = lwm2m_observe( mp_lwm2mH, p_cli->internalID, &uri, notifyObjCb,
                const_cast<LWM2MObject*>( p_obj ));

            if( lwm2mRet != COAP_NO_ERROR )
                ret = -1;
            else
                ret = 0;
        }
        else
        {
            /* cancel observation */
            lwm2mRet = lwm2m_observe_cancel( mp_lwm2mH, p_cli->internalID, &uri, notifyObjCb,
                const_cast<LWM2MObject*>( p_obj ));

            if( lwm2mRet != COAP_NO_ERROR )
                ret = -1;
            else
                ret = 0;
        }
    }

    if( ret == 0 )
    {
        while( true )
        {
            int status;
            OPCUA_LWM2M_SERVER_MUTEX_LOCK(this);

#ifndef OPCUA_LWM2M_SERVER_USE_THREAD
            /* call the server */
            runServer();
#endif /* #ifndef OPCUA_LWM2M_SERVER_USE_THREAD */
            status = p_cbData->status;
            OPCUA_LWM2M_SERVER_MUTEX_UNLOCK(this);
            if( status != -1)
                break;
            OPCUA_LWM2M_SERVER_SLEEP(LWM2MSERVER_RUN_TOT_US);
        }

        if( p_cbData->status == NO_ERROR)
        {
            ret = 0;
            if( observe == false )
            {
                /* observation was canceled so we have to delete the
                 * observe parameters */
              m_obsObjMap.erase( it );
            }
        }
        else
            ret = -1;
    }
    return ret;

} /* LWM2MServer::observe() */



/*---------------------------------------------------------------------------*/
/*
* LWM2MServer::observe()
*/
int8_t LWM2MServer::observe( const LWM2MResource* p_res, bool observe )
{
    int8_t ret = 0;
    lwm2m_client_t* p_cli;
    const LWM2MDevice* p_dev;
    const LWM2MObject* p_obj;
    lwm2m_uri_t uri;

    std::map< const LWM2MResource*, s_lwm2m_obsparams_t*>::iterator it;
    s_lwm2m_obsparams_t* p_cbData = NULL;
    int lwm2mRet;

    if( p_res == NULL )
        /* Invalid arguments */
        ret = -1;

    if( ret == 0 )
    {
        it = m_obsResMap.find( p_res );

        /* get the observe parameters */
        if( it == m_obsResMap.end() )
        {
            if( observe == true )
            {
                /*create a new entry for the resource */
                p_cbData = new s_lwm2m_obsparams_t();

                m_obsResMap.insert(std::pair< const LWM2MResource*, s_lwm2m_obsparams_t* >
                    ( p_res, p_cbData ) );
            }
        }
        else
        {
            /* use existing parameter */
            p_cbData = it->second;
        }
    }

    OPCUA_LWM2M_SERVER_MUTEX_LOCK(this);

    if( ret == 0 )
    {
        if( (!isAlive()) || (p_res == NULL) )
            ret = -1;
    }

    if( ret == 0 )
    {
        /* get object of the resource */
        p_obj = p_res->getObject();
        if( p_obj == NULL )
            ret = -1;
    }

    if( ret == 0 )
    {
        /* get device of the resource */
        p_dev = p_obj->getDevice();
        if( p_dev == NULL )
            ret = -1;
    }

    if( ret == 0 )
    {
        /* find the device in the list of registered devices */
        p_cli = getDevice( p_dev->getName() );
        if( p_cli == NULL )
            ret = -1;
    }

    if( ret == 0 )
    {
        /* start the query with the according values */
        uri.objectId = p_obj->getObjId();
        uri.instanceId = p_obj->getInstId();
        uri.resourceId = p_res->getResId();
        uri.flag = LWM2M_URI_FLAG_OBJECT_ID | LWM2M_URI_FLAG_INSTANCE_ID |
                LWM2M_URI_FLAG_RESOURCE_ID;

        if( observe == true )
        {
            /* start observation */
            p_cbData->status = -1;

            lwm2mRet = lwm2m_observe( mp_lwm2mH, p_cli->internalID, &uri, notifyResCb,
                const_cast<LWM2MResource*>( p_res ));

            if( lwm2mRet != COAP_NO_ERROR )
                ret = -1;
            else
                ret = 0;
        }
        else
        {
            /* cancel observation */
            lwm2mRet = lwm2m_observe_cancel( mp_lwm2mH, p_cli->internalID, &uri, notifyResCb,
                const_cast<LWM2MResource*>( p_res ));

            if( lwm2mRet != COAP_NO_ERROR )
                ret = -1;
            else
                ret = 0;
        }
    }

    if( ret == 0 )
    {
        while( true )
        {
            int status;
            OPCUA_LWM2M_SERVER_MUTEX_LOCK(this);

#ifndef OPCUA_LWM2M_SERVER_USE_THREAD
            /* call the server */
            runServer();
#endif /* #ifndef OPCUA_LWM2M_SERVER_USE_THREAD */
            status = p_cbData->status;
            OPCUA_LWM2M_SERVER_MUTEX_UNLOCK(this);
            if( status != -1)
                break;
            OPCUA_LWM2M_SERVER_SLEEP(LWM2MSERVER_RUN_TOT_US);
        }

        if( p_cbData->status == NO_ERROR)
        {
            ret = 0;
            if( observe == false )
            {
                /* observation was canceled so we have to delete the
                 * observe parameters */
                m_obsResMap.erase( it );
            }
        }
        else
            ret = -1;
    }
    return ret;
} /* LWM2MServer::observe() */


/*---------------------------------------------------------------------------*/
/*
* LWM2MServer::registerObserver()
*/
int8_t LWM2MServer::registerObserver( LWM2MServerObserver* p_observer )
{
    if( p_observer == NULL )
        return -1;

    /* find the observer in the list */
    std::vector< LWM2MServerObserver*>::iterator it =
            m_vectObs.begin();

    while( it != m_vectObs.end() )
    {
        if( *it == p_observer )
            /* found observer */
            break;
        it++;
    }


    if( it == m_vectObs.end() )
    {
        /* The observer was not found so it has to be added. */
        m_vectObs.push_back( p_observer );
    }

    return 0;

} /* LWM2MResource::registerObserver() */



/*---------------------------------------------------------------------------*/
/*
* LWM2MServer::deregisterObserver()
*/
int8_t LWM2MServer::deregisterObserver( const LWM2MServerObserver* p_observer )
{
    if( p_observer == NULL )
        return -1;

    /* find the observer in the list */
    std::vector< LWM2MServerObserver*>::iterator it =
            m_vectObs.begin();

    while( it != m_vectObs.end() )
    {
        if( *it == p_observer )
            /* found observer */
            break;
        it++;
    }


    if( it != m_vectObs.end() )
    {
        /* The observer was not found so it has to be added. */
        m_vectObs.erase( it );
    }

    return 0;

} /* LWM2MResource::deregisterObserver() */


/*---------------------------------------------------------------------------*/
/*
* LWM2MServer::getDevice()
*/
lwm2m_client_t* LWM2MServer::getDevice( std::string client )
{
    if( !isAlive() )
        return NULL;

    /* client pointer */
    lwm2m_client_t * p_targetP;
    p_targetP = mp_lwm2mH->clientList;

    if (p_targetP == NULL)
    {
        return NULL;
    }

    for( p_targetP = mp_lwm2mH->clientList; p_targetP != NULL;
            p_targetP = p_targetP->next )
    {
        if( p_targetP != NULL )
        {
            if( client ==  p_targetP->name)
                return p_targetP;
        }
    }

    return NULL;

} /* LWM2MServer::getDevice() */


/*---------------------------------------------------------------------------*/
/*
* LWM2MServer::notifyObservers()
*/
int8_t LWM2MServer::notifyObservers( s_lwm2m_serverobserver_event_param_t param,
    e_lwm2m_serverobserver_event_t ev ) const
{
    /* Iterate through the observers in the list */
    std::vector< LWM2MServerObserver*>::const_iterator it =
             m_vectObs.begin();

    while( it != m_vectObs.end() )
    {
        /* notify the current observer */
        (*it)->notify( param, ev );
        it++;
    }

    return 0;
} /* LWM2MServer::notifyObservers() */


/*---------------------------------------------------------------------------*/
/*
* LWM2MServer::checkEvents()
*/
void LWM2MServer::checkEvents( void )
{
    OPCUA_LWM2M_SERVER_MUTEX_LOCK(this);
    while( m_devEv.size() )
    {
        s_devEvent_t ev = m_devEv.front();
        m_devEv.pop();
        /* Notify all Observers */
        notifyObservers( ev.param, ev.event );
    }
    OPCUA_LWM2M_SERVER_MUTEX_UNLOCK(this);

} /* LWM2MServer::checkEvents() */


/*---------------------------------------------------------------------------*/
/*
* LWM2MServer::checkDeletedDevices()
*/
void LWM2MServer::checkDeletedDevices( void )
{
    std::list< s_devDel_t >::iterator it = m_devDel.begin();
    while( it != m_devDel.end() )
    {
        if(it->tot < time(NULL))
        {
          /* Timeout expired, delete element */
          if( it->p_dev != NULL )
          {
            /* Delete all the observed resources from the device */
            deletedObserveParams( it->p_dev );
            delete( it->p_dev );
          }

          m_devDel.pop_front();
          it = m_devDel.begin();
        }
        else
        {
          /* Stop looking */
          break;
        }
    }
} /* LWM2MServer::checkDeletedDevices() */



/*---------------------------------------------------------------------------*/
/*
* LWM2MServer::deletedObserveParams()
*/
void LWM2MServer::deletedObserveParams( LWM2MDevice* p_dev )
{
  std::vector< LWM2MObject* >::iterator objIt;
  std::vector< LWM2MResource* >::const_iterator resIt;
  std::map< const LWM2MResource*, s_lwm2m_obsparams_t*>::iterator paramIt;

  OPCUA_LWM2M_SERVER_MUTEX_LOCK(p_srv);
  if( p_dev != NULL)
  {
      objIt = p_dev->objectStart();
      while( objIt != p_dev->objectEnd() )
      {
          resIt = (*objIt)->resourceStart();
          while( resIt != (*objIt)->resourceEnd() )
          {
              paramIt = m_obsResMap.find( *resIt );
              if( paramIt != m_obsResMap.end() )
              {
                  delete( paramIt->second );
                  paramIt = m_obsResMap.erase( paramIt );
              }
              resIt++;
          }
          objIt++;
      }
  }
  OPCUA_LWM2M_SERVER_MUTEX_UNLOCK(p_srv);

} /* LWM2MServer::deletedObserveParams() */


/*---------------------------------------------------------------------------*/
/*
* LWM2MServer::monitorCb()
*/
void LWM2MServer::monitorCb( uint16_t clientID, lwm2m_uri_t * uriP, int status,
        lwm2m_media_type_t format, uint8_t * data, int dataLength,
        void * userData )
{
    int ret = 0;

    std::map< std::string, LWM2MDevice* >::iterator it;
    LWM2MServer* p_srv = (LWM2MServer*)userData;

    OPCUA_LWM2M_SERVER_MUTEX_LOCK(p_srv);

    lwm2m_context_t* lwm2mH = p_srv->mp_lwm2mH;
    lwm2m_client_t* targetP;
    lwm2m_client_object_t * objectP;

    switch( status )
    {
    case COAP_201_CREATED:

        /* A new client was registered */
        targetP = (lwm2m_client_t *)lwm2m_list_find((lwm2m_list_t *)lwm2mH->clientList, clientID);
        if( targetP == NULL )
          ret = -1;

        if( ret == 0 )
        {
          std::map< std::string, LWM2MDevice* >::iterator it = p_srv->m_devMap.find( targetP->name );
          /* check the map for an existing device with the same name */
          if( it != p_srv->m_devMap.end())
          {

            /* The device already exists. Delete the device before creating
             * a new one. */
            s_devEvent_t ev;
            strncpy( (char*)ev.param.devName, it->second->getName().c_str(),
                sizeof(ev.param.devName));
            ev.event = e_lwm2m_serverobserver_event_deregister;
            p_srv->m_devEv.push( ev );

            /* move the device to the deleted device list */
            p_srv->m_devDel.push_back( {it->second, (time(NULL) + (it->second->getLifetime() * 2))} );
            p_srv->m_devMap.erase( it );

            ret = 0;
          }
        }

        if( ret == 0 )
        {
          /* create a new device and add it to the list */
          LWM2MDevice* p_dev = new LWM2MDevice( targetP->name,
              targetP->internalID, p_srv );

          /* add all objects registered at the device */
          for (objectP = targetP->objectList; objectP != NULL ; objectP = objectP->next)
          {
            /* objects without instances are not supported */
            if (objectP->instanceList != NULL)
            {
              /* iterate through all the instances and assign the found objects
              * to the current device */
              lwm2m_list_t * instanceP;

              for (instanceP = objectP->instanceList; instanceP != NULL ;
                  instanceP = instanceP->next)
              {
                /* create a new Object and add it to the device */
                LWM2MObject* p_obj = new LWM2MObject( objectP->id, instanceP->id );
                p_dev->addObject( p_obj );
              }
            }
          }

          p_srv->m_devMap.insert(
              std::pair< std::string, LWM2MDevice* >( p_dev->getName(), p_dev ) );

          /** Add event */
          s_devEvent_t ev;
          strncpy( (char*)ev.param.devName, p_dev->getName().c_str(),
                       sizeof(ev.param.devName));
          ev.event = e_lwm2m_serverobserver_event_register;
          p_srv->m_devEv.push( ev );
        }
        break;

    case COAP_202_DELETED:

        /* An existing client was deleted. */
        it = p_srv->m_devMap.begin();
        while( it != p_srv->m_devMap.end() )
        {
          if( it->second->getID() == clientID )
            /* found matching ID */
            break;
          it++;
        }

        /* check the map for an existing device with the same name */
        if( it == p_srv->m_devMap.end())
          ret = -1;

        if( ret == 0 )
        {
          /* Notify all Observers */
          s_devEvent_t ev;
          strncpy( (char*)ev.param.devName, it->second->getName().c_str(),
              sizeof(ev.param.devName));
          ev.event = e_lwm2m_serverobserver_event_deregister;
          p_srv->m_devEv.push( ev );

          /* move the device to the deleted device list */
          p_srv->m_devDel.push_back( {it->second, (time(NULL) + (it->second->getLifetime() * 2))} );
          p_srv->m_devMap.erase( it );
        }
        break;

    case COAP_204_CHANGED:

        /* An existing client was updated. */
        targetP = (lwm2m_client_t *)lwm2m_list_find((lwm2m_list_t *)lwm2mH->clientList,
            clientID);

        /** TODO */

        break;

    default:
        /* unhandled status */
        break;
    }

    OPCUA_LWM2M_SERVER_MUTEX_UNLOCK(p_srv);

};


/*---------------------------------------------------------------------------*/
/*
* LWM2MServer::readWriteCb()
*/
void LWM2MServer::readWriteResCb( uint16_t clientID, lwm2m_uri_t * uriP, int status,
        lwm2m_media_type_t format, uint8_t * data, int dataLength,
        void * userData )
{
    int ret = 0;
    lwm2m_data_t* p_lwm2mData;

    /* convert user data to server instance */
    s_lwm2m_obsparams_t* p_cbParams = (s_lwm2m_obsparams_t*)userData;

    LWM2MServer* p_srv = LWM2MServer::instance();
    LWM2MDevice* p_dev = NULL;
    LWM2MObject* p_obj = NULL;
    LWM2MResource* p_res = NULL;

    OPCUA_LWM2M_SERVER_MUTEX_LOCK(p_srv);

    /* set LWM2M parameters */
    p_cbParams->clientID = clientID;
    p_cbParams->uriP = uriP;
    p_cbParams->status = status;
    p_cbParams->format = format;
    p_cbParams->data = NULL;
    p_cbParams->buffer = data;
    p_cbParams->bufferLen = dataLength;

    std::map< std::string, LWM2MDevice* >::const_iterator devIt = p_srv->m_devMap.begin();
    while( devIt != p_srv->m_devMap.end() )
    {
      if( devIt->second->getID() == p_cbParams->clientID )
      {
        p_dev = devIt->second;
        break;
      }
      devIt++;
    }

    if( p_dev == NULL )
    {
      OPCUA_LWM2M_SERVER_MUTEX_UNLOCK(p_srv);
      return;
    }

    if( LWM2M_URI_IS_SET_INSTANCE( p_cbParams->uriP ) )
    {
      /* get object */
      p_obj = p_dev->getObject( p_cbParams->uriP->objectId,
          p_cbParams->uriP->instanceId );
    }

    if( (p_obj != NULL) && ( LWM2M_URI_IS_SET_RESOURCE( p_cbParams->uriP ) ) )
    {
      /* Get the resource */
      p_res = p_obj->getResource( p_cbParams->uriP->resourceId );
    }

    if( p_res != NULL )
    {
        ret = lwm2m_data_parse( p_cbParams->uriP, p_cbParams->buffer,
               p_cbParams->bufferLen, p_cbParams->format, &p_lwm2mData );

        if( ret > 0 )
          p_cbParams->data = p_lwm2mData;
        p_cbParams->dataLen = ret;
    }
    else if( p_obj != NULL )
    {
      ret = lwm2m_data_parse( p_cbParams->uriP, p_cbParams->buffer,
             p_cbParams->bufferLen, p_cbParams->format, &p_lwm2mData );

      /* Iterate through the resources and notify about the change of data */
      std::vector< LWM2MResource* >::const_iterator it = p_obj->resourceStart();
      p_cbParams->buffer = NULL;
      while( it != p_obj->resourceEnd() )
      {
          lwm2m_data_t* p_lwm2mDataCur = p_lwm2mData;
          for( int i = 0; i < ret; i++ )
          {
              if( p_lwm2mData->id == (*it)->getResId() )
              {
                  /* ID match */
                  p_cbParams->data = p_lwm2mDataCur;
                  /* notify */
                  (*it)->notifyObservers( p_cbParams );
              }
              p_lwm2mData++;
          }
          it++;
      }

      lwm2m_data_free(ret, p_lwm2mData);
    }

    OPCUA_LWM2M_SERVER_MUTEX_UNLOCK(p_srv);
};


/*---------------------------------------------------------------------------*/
/*
* LWM2MServer::notifyResCb()
*/
void LWM2MServer::notifyResCb( uint16_t clientID, lwm2m_uri_t * uriP, int status,
        lwm2m_media_type_t format, uint8_t * data, int dataLength,
        void * userData )
{
    int ret = 0;
    lwm2m_data_t* p_lwm2mData;
    std::map< const LWM2MResource*, s_lwm2m_obsparams_t*>::iterator it;
    s_lwm2m_obsparams_t* p_cbParams;

    LWM2MServer* p_srv = LWM2MServer::instance();
    LWM2MDevice* p_dev = NULL;
    LWM2MObject* p_obj = NULL;
    LWM2MResource* p_res = NULL;

    OPCUA_LWM2M_SERVER_MUTEX_LOCK(p_srv);

    it = p_srv->m_obsResMap.find( (const LWM2MResource*)userData );
    if( it == p_srv->m_obsResMap.end() )
    {
      /* No observe parameters found for the resource */
      OPCUA_LWM2M_SERVER_MUTEX_UNLOCK(p_srv);
      return;
    }
    else
    {
      /* set parameter pointer */
      p_cbParams = it->second;
    }

    /* set LWM2M parameters */
    p_cbParams->clientID = clientID;
    p_cbParams->uriP = uriP;
    p_cbParams->status = status;
    p_cbParams->format = format;
    p_cbParams->data = NULL;
    p_cbParams->buffer = data;
    p_cbParams->bufferLen = dataLength;


    std::map< std::string, LWM2MDevice* >::const_iterator devIt = p_srv->m_devMap.begin();
    while( devIt != p_srv->m_devMap.end() )
    {
      if( devIt->second->getID() == p_cbParams->clientID )
      {
        p_dev = devIt->second;
        break;
      }
      devIt++;
    }

    if( p_dev == NULL )
    {
      OPCUA_LWM2M_SERVER_MUTEX_UNLOCK(p_srv);
      return;
    }

    if( LWM2M_URI_IS_SET_INSTANCE( p_cbParams->uriP ) )
    {
      /* get object */
      p_obj = p_dev->getObject( p_cbParams->uriP->objectId,
          p_cbParams->uriP->instanceId );

    }

    if( (p_obj != NULL) && ( LWM2M_URI_IS_SET_RESOURCE( p_cbParams->uriP ) ) )
    {
      /* Get the resource */
      p_res = p_obj->getResource( p_cbParams->uriP->resourceId );
    }

    if( p_res != NULL )
    {
        ret = lwm2m_data_parse( p_cbParams->uriP, p_cbParams->buffer,
               p_cbParams->bufferLen, p_cbParams->format, &p_lwm2mData );

        if( ret > 0 )
          p_cbParams->data = p_lwm2mData;

        /* call the notification */
        p_res->notifyObservers( p_cbParams );
        if( p_lwm2mData != NULL )
          lwm2m_data_free(ret, p_lwm2mData);

    }
    OPCUA_LWM2M_SERVER_MUTEX_UNLOCK(p_srv);
};


/*---------------------------------------------------------------------------*/
/*
* LWM2MServer::notifyObjCb()
*/
void LWM2MServer::notifyObjCb( uint16_t clientID, lwm2m_uri_t * uriP, int status,
        lwm2m_media_type_t format, uint8_t * data, int dataLength,
        void * userData )
{
    int ret = 0;
    lwm2m_data_t* p_lwm2mData;
    std::map< const LWM2MObject*, s_lwm2m_obsparams_t*>::iterator it;
    s_lwm2m_obsparams_t* p_cbParams;

    LWM2MServer* p_srv = LWM2MServer::instance();
    LWM2MDevice* p_dev = NULL;
    LWM2MObject* p_obj = NULL;

    OPCUA_LWM2M_SERVER_MUTEX_LOCK(p_srv);

    it = p_srv->m_obsObjMap.find( (const LWM2MObject*)userData );
    if( it == p_srv->m_obsObjMap.end() )
    {
      /* No observe parameters found for the resource */
      OPCUA_LWM2M_SERVER_MUTEX_UNLOCK(p_srv);
      return;
    }
    else
    {
      /* set parameter pointer */
      p_cbParams = it->second;
    }

    /* set LWM2M parameters */
    p_cbParams->clientID = clientID;
    p_cbParams->uriP = uriP;
    p_cbParams->status = status;
    p_cbParams->format = format;
    p_cbParams->data = NULL;
    p_cbParams->buffer = data;
    p_cbParams->bufferLen = dataLength;


    std::map< std::string, LWM2MDevice* >::const_iterator devIt = p_srv->m_devMap.begin();
    while( devIt != p_srv->m_devMap.end() )
    {
      if( devIt->second->getID() == p_cbParams->clientID )
      {
        p_dev = devIt->second;
        break;
      }
      devIt++;
    }

    if( p_dev == NULL )
    {
      OPCUA_LWM2M_SERVER_MUTEX_UNLOCK(p_srv);
      return;
    }

    if( LWM2M_URI_IS_SET_INSTANCE( p_cbParams->uriP ) )
    {
      /* get object */
      p_obj = p_dev->getObject( p_cbParams->uriP->objectId,
          p_cbParams->uriP->instanceId );
    }

    if( p_obj != NULL )
    {
      ret = lwm2m_data_parse( p_cbParams->uriP, p_cbParams->buffer,
             p_cbParams->bufferLen, p_cbParams->format, &p_lwm2mData );

      /* Iterate through the resources and notify about the change of data */
      std::vector< LWM2MResource* >::const_iterator it = p_obj->resourceStart();
      p_cbParams->buffer = NULL;
      while( it != p_obj->resourceEnd() )
      {
          lwm2m_data_t* p_lwm2mDataCur = p_lwm2mData;
          for( int i = 0; i < ret; i++ )
          {
              if( p_lwm2mDataCur->id == (*it)->getResId() )
              {
                  /* ID match */
                  p_cbParams->data = p_lwm2mDataCur;
                  /* notify */
                  (*it)->notifyObservers( p_cbParams );
              }
              p_lwm2mDataCur++;
          }
          it++;
      }

      lwm2m_data_free(ret, p_lwm2mData);
    }

    OPCUA_LWM2M_SERVER_MUTEX_UNLOCK(p_srv);
};

/* initialization of the static member */
LWM2MServer* LWM2MServer::m_instance = NULL;


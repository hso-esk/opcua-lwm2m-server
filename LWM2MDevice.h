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
 * \file    LWM2MDevice.h
 * \author  Institute of reliable Embedded Systems
 *          and Communication Electronics
 * \date    $Date$
 * \version $Version$
 *
 * \brief   Description of a LWM2M Device.
 *
 */


#ifndef __LWM2MDEVICE_H__
#define __LWM2MDEVICE_H__
#ifndef __DECL_LWM2MDEVICE_H__
#define __DECL_LWM2MDEVICE_H__ extern
#endif /* #ifndef __DECL_LWM2MDEVICE_H__ */


/*
 * --- Includes ------------------------------------------------------------- *
 */
#include <stdint.h>
#include <iostream>
#include <string>
#include <vector>
#include "LWM2MObject.h"
#include "liblwm2m.h"

/*
 * --- Forward Declaration ----------------------------------------------------- *
 */

/* Forward declaration of the LWM2MServer class used as server reference. */
class LWM2MServer;

/*
 * --- Class Definition ----------------------------------------------------- *
 */

/**
 * \brief   LWM2MDevice Class.
 *
 *          A LWM2M Device represents a LWM2M client which can have several
 *          objects and which is identified by a specific name.
 */
class LWM2MDevice
{
    friend class LWM2MObject;
    friend class LWM2MServer;

public:

    /**
     * \brief   Default constructor to create a LWM2M Object.
     */
    LWM2MDevice( std::string name, uint16_t id, LWM2MServer* p_srv )
        : m_name( name )
        , m_id( id )
        , mp_srv( p_srv ){

        /* clear object vector */
        m_objVect.clear();
    };


    /**
     * \brief   Default destructor of the LWM2M Device.
     */
    virtual ~LWM2MDevice( void ) {

        std::vector< LWM2MObject* >::iterator it = m_objVect.begin();

        /*delete all Objects */
        while( it != m_objVect.end() )
        {
            delete (*it);
            it++;
        }
    };


    /**
     * \brief   Get the name of the device.
     *
     * \return  Name of the device.
     */
    std::string getName( void ) const {return m_name;}


    /**
     * \brief   Get the internal ID of the device.
     *
     * \return  Internal of the device.
     */
    uint16_t getID( void ) const {return m_id;}


    /**
     * \brief   Get the lifetime of the device.
     *
     *          Each device has a lifetime. If there was no communication
     *          within that time the device is removed from the server.
     *
     * \return  The lifetime on success or a negative value on error.
     */
    int32_t getLifetime( void );


    /**
     * \brief   Get the end of life of the device.
     *
     *          Each device has a lifetime. If there was no communication
     *          within that time the device is removed from the server.
     *
     * \return  The end of life on success or a negative value on error.
     */
    int32_t getEndOfLife( void );


    /**
     * \brief   Get a specific object.
     *
     * \param   objID   ID of the object.
     * \param   instID  ID of the instance of the object.
     *
     * \return  Pointer to the object if it exists or NULL otherwise.
     */
    LWM2MObject* getObject( uint16_t objID, uint8_t instID );


    /**
     * \brief   Get the begin of the registered objects.
     *
     * \return  Iterator pointing to the begin of the objects.
     */
    std::vector< LWM2MObject* >::iterator objectStart( void ) {
        return m_objVect.begin();
    };


    /**
     * \brief   Get the end of the registered objects.
     *
     * \return  Iterator pointing to the end of the objects.
     */
    std::vector< LWM2MObject* >::iterator objectEnd( void ) {
        return m_objVect.end();
    };


    /**
     * \brief   Get the associated server object.
     *
     * \return  Parent object.
     */
    LWM2MServer* getServer( void ) const {return mp_srv;}


protected:

    /**
     * \brief   Add a new object to the list.
     *
     * \param   p_obj  Object to add.
     *
     * \return  0 on success or negative value on error.
     */
    int16_t addObject( LWM2MObject* p_obj );

private:

    /** Name of the device */
    std::string m_name;

    /** ID of the device */
    uint16_t m_id;

    /** Vector of resources */
    std::vector< LWM2MObject* > m_objVect;

    /** Server instance this device belongs to */
    LWM2MServer* mp_srv;
};

#endif /* #ifndef __LWM2MDEVICE_H__ */


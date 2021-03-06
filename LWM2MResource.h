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
 * \file    LWM2MResource.h
 * \author  Institute of reliable Embedded Systems
 *          and Communication Electronics
 * \date    $Date$
 * \version $Version$
 *
 * \brief   Description of a LWM2M Resource.
 *
 */


#ifndef __LWM2MRESOURCE_H__
#define __LWM2MRESOURCE_H__
#ifndef __DECL_LWM2MRESOURCE_H__
#define __DECL_LWM2MRESOURCE_H__ extern
#endif /* #ifndef __LWM2MRESOURCE_H__ */


/*
 * --- Includes ------------------------------------------------------------- *
 */
#include <stdint.h>
#include <iostream>
#include <string>
#include <vector>
#include "LWM2MResourceObserver.h"

/*
 * --- Forward Declaration ----------------------------------------------------- *
 */

/* Forward declaration of the LWM2MServer class. */
class LWM2MServer;
/* Forward declaration of the LWM2MDevice class used as parent reference. */
class LWM2MDevice;
/* Forward declaration of the LWM2MObject class used as parent reference. */
class LWM2MObject;

/*
 * --- Class Definition ----------------------------------------------------- *
 */

/**
 * \brief   LWM2MResource Class.
 *
 *          A LWM2M Resource specifies a single resource within an object. A
 *          resource can be readable writable or executable;
 */
class LWM2MResource
{
    friend class LWM2MObject;
    friend class LWM2MServer;

public:

    /**
     * \brief   Default constructor to create a LWM2M Object.
     */
    LWM2MResource( void )
        : m_resId( 0 )
        , mp_parent( NULL ) {

        /* clear the observer vector */
        m_vectObs.clear();
    };


    /**
     * \brief   Extended constructor to create a LWM2M Object.
     */
    LWM2MResource( uint16_t resId, bool rd = false, bool wr  = false,
            bool ex  = false )
        : m_resId( resId )
        , mp_parent( NULL ) {

        /* clear the observer vector */
        m_vectObs.clear();
    };


    /**
     * \brief   Default destructor of the LWM2M Server.
     */
    virtual ~LWM2MResource( void ) {};


    /**
     * \brief   Get the resource ID.
     *
     * \return  Resource ID.
     */
    uint16_t getResId( void ) const {return m_resId;}


    /**
     * \brief   Get the parent object.
     *
     * \return  Parent object.
     */
    const LWM2MObject* getObject( void ) const {return mp_parent;}


    /**
     * \brief   Get Device instance.
     *
     * \return  Device object.
     */
    const LWM2MDevice* getDevice( void ) const;


    /**
     * \brief   Get server instance.
     *
     * \return  Server object.
     */
    LWM2MServer* getServer( void ) const;


    /**
     * \brief   Register an observer at the resource.
     *
     *          An observer that is registered at the resource will be
     *          notified about every event (e.g. value changed) of
     *          the resource.
     *
     * \param   p_observer  Observer that shall be registered.
     *
     * \return  0 on success or negative value on error.
     */
    int8_t registerObserver( LWM2MResourceObserver* p_observer );


    /**
     * \brief   Deregister a registered Observer at the resource.
     *
     *          An observer that is deregistered at the resource will be
     *          deleted from the list and will not receive any notifications.
     *
     * \param   p_observer  Observer that shall be deregistered.
     *
     * \return  0 on success or negative value on error.
     */
    int8_t deregisterObserver( const LWM2MResourceObserver* p_observer );


protected:

    /**
     * \brief   Set the parent object.
     *
     * \param   p_parent  Parent object.
     */
    void setParent( const LWM2MObject* p_parent ) {mp_parent = p_parent;};


    /**
     * \brief   Check if the resource has observers.
     *
     * \return  True if the resource has observers of false otherwise.
     */
    bool hasObserver( void ) const { return m_vectObs.size(); };


    /**
     * \brief   Notify all observers about a change in the resource.
     *
     * \param   p_params  LWM2M parameters of the notification.
     *
     * \return  0 on success or negative value on error.
     */
    int8_t notifyObservers( const s_lwm2m_obsparams_t* p_params ) const;


private:

    /** Resource ID */
    uint16_t m_resId;

    /** parent object */
    const LWM2MObject* mp_parent;

    /** Vector of registed observer */
    std::vector< LWM2MResourceObserver* > m_vectObs;

};

#endif /* #ifndef __LWM2MRESOURCE_H__ */


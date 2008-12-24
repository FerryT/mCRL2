// Author(s): Diana Koenraadt, Remco Blewanus, Bram Schoenmakers, Thorstin Crijns, Hans Poppelaars, Bas Luksenburg, Jonathan Nelisse
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
/// \file architecturediagram.h
//
// Declares the architecture_diagram class.

#ifndef LIBGRAPE_ARCHITECTUREDIAGRAM_H
#define LIBGRAPE_ARCHITECTUREDIAGRAM_H

#include "diagram.h"
#include "blocked.h"
#include "visible.h"
#include "channelcommunication.h"
#include "architecturereference.h"

namespace grape
{
  namespace libgrape
  {

    /**
     * \short Represents an architecture diagram.
     * A description of the architecture of the "modelled system". An
     * Architecture Diagram has a Visibility Frame, that determines
     * what behavior of the diagram is externally visible. An Archi-
     * tecture Diagram contains a number of Process References and
     * Architecture References and the Channels they share.
     *
     * @see diagram
     */
    class architecture_diagram : public diagram
    {
      protected:
        arr_blocked                 m_blocked;                /**< blocked connection properties in this architecture diagram. */
        arr_visible                 m_visibles;               /**< visible connection properties in this architecture diagram. */
        arr_channel_communication   m_channel_communications;  /**< channel communications in this architecture diagram. */
        arr_channel                 m_channels;               /**< channels in this architecture diagram. */
        arr_architecture_reference  m_architecture_references; /**< architecture references in this architecture diagram. */
        arr_process_reference       m_process_references;     /**< process references in this architecture diagram. */

      public:
        /**
         * Default constructor.
         * Initializes architecture_diagram.
         */
        architecture_diagram( void );

        /**
         * Copy constructor.
         * Creates a new architecture diagram based on an existing one.
         * @param p_arch_diagram The architecture diagram to be copied.
         */
        architecture_diagram( const architecture_diagram &p_arch_diagram );

        /**
         * Default destructor.
         * Frees allocated memory.
         */
        ~architecture_diagram( void );

        /**
         * Architecture reference addition function.
         * Deselects all objects in the diagram and creates a new architecture reference.
         * @param p_id The id of the architecture reference.
         * @param p_coord The coordinate at which the architecture reference should be created.
         * @param p_def_width The default width of an architecture reference.
         * @param p_def_height The default height of an architecture reference.
         * @return Returns a pointer to the newly created architecture reference.
         */
        architecture_reference* add_architecture_reference( unsigned int p_id, coordinate &p_coord, float p_def_width, float p_def_height );

        /**
         * Architecture reference removal function.
         * Removes the architecture reference and all associated channels and properties associated with those channels.
         * Disconnects the architecture reference from all channel communications and removes these if necessary.
         * @param p_arch_ref The architecture reference.
         */
        void remove_architecture_reference( architecture_reference* p_arch_ref );

        /**
         * Architecture reference count function.
         * @return Returns the number of architecture references in the architecture diagram.
         */
        unsigned int count_architecture_reference( void );

        /**
         * Architecture reference retrieval function.
         * @param p_i The index of the to be retrieved architecture reference.
         * @return Returns a pointer to the architecture reference at index p_i in the list of architecture reference.
         */
        architecture_reference* get_architecture_reference( int p_i );

        /**
         * To be used exclusively during XML-conversion.
         * @return A pointer to list of architecture references
         */
        arr_architecture_reference* get_architecture_reference_list( void );

        /**
         * Process reference addition function.
         * Deselects all objects in the diagram and creates a new process reference.
         * @param p_id The id of the architecture reference.
         * @param p_coord The coordinate at which the process reference should be created.
         * @param p_def_width The default width of a process reference.
         * @param p_def_height The default height of a process reference.
         * @return Returns a pointer to the newly created process reference.
         */
        process_reference* add_process_reference( unsigned int p_id, coordinate &p_coord, float p_def_width, float p_def_height );

        /**
         * Process reference removal function.
         * Removes the process reference and all associated channels and properties associated with those channels.
         * Disconnects the process reference from all channel communications and removes these if necessary.
         * @param p_proc_ref The process reference.
         */
        void remove_process_reference( process_reference* p_proc_ref );

        /**
         * Process reference count function.
         * @return Returns the number of process references in the architecture diagram.
         */
        unsigned int count_process_reference( void );

        /**
         * Process reference retrieval function.
         * @param p_i The index of the to be retrieved process reference.
         * @return Returns a pointer to the process reference at index p_i in the list of process reference.
         */
        process_reference* get_process_reference( int p_i );

        /**
         * To be used exclusively during XML-conversion.
         * @return A pointer to list of process references
         */
        arr_process_reference* get_process_reference_list( void );

        /**
         * Channel addition function.
         * Deselects all objects in the diagram and creates a new channel.
         * @param p_id The id of the channel.
         * @param p_coord The coordinate at which the channel should be created.
         * @param p_def_width The default width of a channel.
         * @param p_def_height The default height of a channel.
         * @param p_ref The reference the channel is attached to.
         * @return Returns a pointer to the newly created channel.
         */
        channel* add_channel( unsigned int p_id, coordinate &p_coord, float p_def_width, float p_def_height, compound_reference* p_ref );

        /**
         * Channel removal function.
         * Removes the channel and all properties associated with it.
         * Disconnects the channel from the channel communication.
         * @param p_channel The channel.
         */
        void remove_channel( channel* p_channel );

        /**
         * Channel count function.
         * @return Returns the number of channels in the architecture diagram.
         */
        unsigned int count_channel( void );

        /**
         * Channel retrieval function.
         * @param p_i The index of the to be retrieved channel.
         * @return Returns a pointer to the channel at index p_i in the list of channels.
         */
        channel* get_channel( int p_i );

        /**
         * To be used exclusively during XML-conversion.
         * @return A pointer to list of channels
         */
        arr_channel* get_channel_list( void );


        /**
         * Channel communication addition function.
         * Deselects all objects in the diagram and creates a new channel communication.
         * @param p_id The id of the channel communication.
         * @param p_coord The coordinate at which the channel communication should be created.
         * @param p_channel_1 The channel involved in the channel communication.
         * @param p_channel_2 The channel involved in the channel communication.
         * @return Returns a pointer to the newly created channel communication.
         */
        channel_communication* add_channel_communication( unsigned int p_id, coordinate &p_coord, channel* p_channel_1, channel* p_channel_2 );

        /**
         * Channel communication removal function.
         * Removes the channel communication, disconnects the channels from the channel communication and removes the properties of the channel communication.
         * @param p_c_comm The channel communication.
         */
        void remove_channel_communication( channel_communication* p_c_comm );

        /**
         * Channel communication count function.
         * @return Returns the number of channel communication in the architecture diagram.
         */
        unsigned int count_channel_communication( void );

        /**
         * Blocked retrieval function.
         * @param p_i The index of the to be retrieved blocked.
         * @return Returns a pointer to the blocked at index p_i in the list of blocked
         */
        channel_communication* get_channel_communication( int p_i );

        /**
         * To be used exclusively during XML-conversion.
         * @return A pointer to list of channel communications
         */
        arr_channel_communication* get_channel_communication_list( void );

        /**
         * Channel attachment function.
         * Attaches the specified channel to the specified channel communication.
         * Detaches the channel from an existing channel communication or connection property first, if necessary.
         * @pre The channel and channel communication are both in this diagram.
         * @param p_chan The channel.
         * @param p_c_comm The channel communication.
         */
        void attach_channel_communication_to_channel( channel_communication* p_c_comm, channel* p_chan );

        /**
         * Channel detachment function.
         * Detaches the specified channel from the channel communication it is currently in.
         * @param p_chan The channel.
         */
        void detach_channel_from_channel_communication( channel* p_chan );

        /**
         * Connection property addition function.
         * Deselects all objects in the diagram and creates a new connection property.
         * @param p_id The id of the connection property.
         * @param p_coord The coordinate at which the connection property should be created.
         * @param p_def_width The default width of a connection property.
         * @param p_def_height The default height of a connection property.
         * @param p_conn The connection to attach this property to.
         * @return Returns a pointer to the newly created blocked.
         */
        blocked* add_blocked( unsigned int p_id, coordinate &p_coord, float p_def_width, float p_def_height, connection* p_conn );

        /**
         * Connection property removal function.
         * Removes the connection property.
         * @param p_blocked The connection property.
         */
        void remove_blocked( blocked* p_blocked );

        /**
         * Blocked count function.
         * @return Returns the number of blocked in the architecture diagram.
         */
        unsigned int count_blocked( void );

        /**
         * Blocked retrieval function.
         * @param p_i The index of the to be retrieved blocked.
         * @return Returns a pointer to the blocked at index p_i in the list of blocked
         */
        blocked* get_blocked( int p_i );

        /**
         * To be used exclusively during XML-conversion.
         * @return A pointer to list of blockeds
         */
        arr_blocked* get_blocked_list( void );

        /**
         * Connection property addition function.
         * Deselects all objects in the diagram and creates a new connection property.
         * @param p_id The id of the connection property.
         * @param p_coord The coordinate at which the connection property should be created.
         * @param p_def_width The default width of a connection property.
         * @param p_def_height The default height of a connection property.
         * @param p_conn The connection to attach the property to.
         * @return Returns a pointer to the newly created visible.
         */
        visible* add_visible( unsigned int p_id, coordinate &p_coord, float p_def_width, float p_def_height, connection* p_conn );

        /**
         * Connection property removal function.
         * Removes the connection property.
         * @param p_visible The connection property.
         */
        void remove_visible( visible* p_visible );

        /**
         * Visible count function.
         * @return Returns the number of visible in the architecture diagram.
         */
        unsigned int count_visible( void );

        /**
         * Visible retrieval function.
         * @param p_i The index of the to be retrieved visible.
         * @return Returns a pointer to the visible at index p_i in the list of visible
         */
        visible* get_visible( int p_i );

        /**
         * To be used exclusively during XML-conversion.
         * @return A pointer to list of visibles
         */
        arr_visible* get_visible_list( void );

        /**
         * Connection property attachment function.
         * Attaches the specified connection property to the specified connection.
         * Detaches existing properties from the connection.
         * Detaches existing channel communications from the connection, if the connection is a channel.
         * @pre The property and connection are both in this diagram.
         * @param p_prop The connection property.
         * @param p_connection The connection.
         */
        void attach_property_to_connection( connection_property* p_prop, connection* p_connection );

        /**
         * Connection property detachment function.
         * Detaches the specified connection property from its connection.
         * @param p_prop The connection property.
         */
        void detach_property_from_connection( connection_property* p_prop );

        /**
         * Object selection function.
         * Selects all objects in the diagram.
         */
        void select_all_objects( void );

        /**
         * Object deselection function.
         * Deselects all objects in the diagram.
         */
        void deselect_all_objects();

        /**
         * Object retrieval function.
         * @param p_arch_dia The architecture diagram to investigate.
         * @param p_id The identifier of the desired object.
         * @param p_type The type of the object you want to retrieve.
         * @return Returns the object that has the specified id.
        */
        static object* find_object( architecture_diagram* p_arch_dia, unsigned int p_id, object_type p_type = ANY );
      private:
        /**
         * Connection property removal function.
         * Performs a dynamic cast on the specified pointer and removes the object from the corresponding list, which is either the list of visibles or the list of blocked.
         */
        void delete_property( connection_property* p_prop );
    };

    /**
     * Array of architecture_diagram.
     */
    WX_DECLARE_OBJARRAY( architecture_diagram, arr_architecture_diagram );


  } // namespace libgrape
} // namespace grape

#endif // LIBGRAPE_ARCHITECTUREDIAGRAM_H

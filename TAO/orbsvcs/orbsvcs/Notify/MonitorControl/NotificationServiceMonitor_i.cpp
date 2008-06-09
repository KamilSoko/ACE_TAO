// $Id$

#include "orbsvcs/orbsvcs/Notify/MonitorControl/NotificationServiceMonitor_i.h"

#if defined (ACE_HAS_MONITOR_FRAMEWORK) && (ACE_HAS_MONITOR_FRAMEWORK == 1)

#include "ace/Auto_Ptr.h"
#include "ace/Monitor_Point_Registry.h"

#include "orbsvcs/orbsvcs/Notify/MonitorControl/Control_Registry.h"
#include "orbsvcs/orbsvcs/Notify/MonitorControl/Statistic.h"

using namespace ACE_VERSIONED_NAMESPACE_NAME::ACE::Monitor_Control;

TAO_BEGIN_VERSIONED_NAMESPACE_DECL

NotificationServiceMonitor_i::NotificationServiceMonitor_i (CORBA::ORB_ptr orb)
  : orb_ (CORBA::ORB::_duplicate (orb))
{
}

Monitor::NameList*
NotificationServiceMonitor_i::get_statistic_names (void)
{
  Monitor_Control_Types::NameList name_list =
    Monitor_Point_Registry::instance ()->names ();
  CORBA::ULong the_length = name_list.size ();

  Monitor::NameList* the_names = 0;
  ACE_NEW_RETURN (the_names,
                  Monitor::NameList (the_length),
                  0);
  Monitor::NameList_var safe_names = the_names;
  the_names->length (the_length);
  CORBA::ULong i = 0;

  for (Monitor_Control_Types::NameList::Iterator iter (name_list);
       !iter.done ();
       iter.advance (), ++i)
    {
      ACE_CString* tmp = 0;
      iter.next (tmp);
      safe_names[i] = tmp->c_str ();
    }

  return safe_names._retn ();
}

CosNotification::NotificationServiceMonitorControl::Data*
NotificationServiceMonitor_i::get_statistic (const char* name)
{
  Monitor_Point_Registry* registry = Monitor_Point_Registry::instance ();

  Monitor::NameList invalid;
  Monitor::NameList names (1);
  names.length (1);
  names[0] = name;
  this->get_invalid_names (registry, names, invalid);

  if (invalid.length () > 0)
    {
      throw CosNotification::NotificationServiceMonitorControl::InvalidName (invalid);
    }

  CosNotification::NotificationServiceMonitorControl::Data* data = 0;
  ACE_NEW_THROW_EX (data,
                    CosNotification::NotificationServiceMonitorControl::Data,
                    CORBA::NO_MEMORY ());
  this->get_data (registry, name, *data);
  return data;
}

CosNotification::NotificationServiceMonitorControl::DataList*
NotificationServiceMonitor_i::get_statistics (const Monitor::NameList& names)
{
  Monitor_Point_Registry* registry = Monitor_Point_Registry::instance ();

  Monitor::NameList invalid;
  this->get_invalid_names (registry, names, invalid);

  if (invalid.length () > 0)
    {
      throw CosNotification::NotificationServiceMonitorControl::InvalidName (invalid);
    }

  CORBA::ULong length = names.length ();
  CosNotification::NotificationServiceMonitorControl::DataList* data = 0;
  ACE_NEW_RETURN (data,
                  CosNotification::NotificationServiceMonitorControl::DataList (
                    length),
                  0);
  ACE_Auto_Basic_Ptr<CosNotification::NotificationServiceMonitorControl::DataList>
    safe_data (data);

  data->length (length);

  for (CORBA::ULong i = 0; i < length; ++i)
    {
      this->get_data (registry, names[i], (*data)[i]);
    }

  return safe_data.release ();
}

CosNotification::NotificationServiceMonitorControl::DataList*
NotificationServiceMonitor_i::get_and_clear_statistics (
  const Monitor::NameList& names)
{
  CosNotification::NotificationServiceMonitorControl::DataList* data =
    this->get_statistics (names);

  // We've gotten to the point where we have a list of data.
  // It is technically possible for the list of statistics names
  // to change at this point.  So, I'm bypassing the call to
  // clear_statistics() to avoid a possible exception.
  CORBA::ULong length = names.length();
  Monitor_Point_Registry* registry = Monitor_Point_Registry::instance ();

  for (CORBA::ULong i = 0; i < length; ++i)
    {
      Monitor_Base* statistic = registry->get (names[i].in ());

      if (statistic != 0)
        {
          statistic->clear ();
        }
    }

  return data;
}

void
NotificationServiceMonitor_i::clear_statistics (
  const Monitor::NameList& names)
{
  Monitor_Point_Registry* registry = Monitor_Point_Registry::instance ();

  Monitor::NameList invalid;
  this->get_invalid_names (registry, names, invalid);

  if (invalid.length () > 0)
    {
      throw CosNotification::NotificationServiceMonitorControl::InvalidName (invalid);
    }

  CORBA::ULong length = names.length ();

  for (CORBA::ULong i = 0; i < length; ++i)
    {
      Monitor_Base* statistic = registry->get (names[i].in ());

      if (statistic != 0)
        {
          statistic->clear ();
        }
    }
}

void
NotificationServiceMonitor_i::shutdown_event_channel (const char* name)
{
  this->send_control_command (name, TAO_NS_CONTROL_SHUTDOWN);
}

void
NotificationServiceMonitor_i::remove_consumer (const char* name)
{
  this->send_control_command (name, TAO_NS_CONTROL_REMOVE_CONSUMER);
}

void
NotificationServiceMonitor_i::remove_supplier (const char* name)
{
  this->send_control_command (name, TAO_NS_CONTROL_REMOVE_SUPPLIER);
}

void
NotificationServiceMonitor_i::remove_consumeradmin (const char* name)
{
  this->send_control_command (name, TAO_NS_CONTROL_REMOVE_CONSUMERADMIN);
}

void
NotificationServiceMonitor_i::remove_supplieradmin (const char* name)
{
  this->send_control_command (name, TAO_NS_CONTROL_REMOVE_SUPPLIERADMIN);
}

void
NotificationServiceMonitor_i::send_control_command (const char* name,
                                                    const char* cmd)
{
  TAO_Control_Registry* instance = TAO_Control_Registry::instance ();
  TAO_NS_Control* control = instance->get (name);

  // If we didn't find a control object with the given name, or the
  // execution of the control object failed, we must throw an exception.
  // The control object execution should only return false when the
  // command given does not correspond to one that is supported by the
  // control object.
  if (control == 0 || !control->execute (cmd))
    {
      Monitor::NameList invalid (1);
      invalid.length (1);
      invalid[0] = name;
      throw CosNotification::NotificationServiceMonitorControl::InvalidName (invalid);
    }
}

void
NotificationServiceMonitor_i::shutdown (void)
{
  if (!CORBA::is_nil (this->orb_.in ()))
    {
      this->orb_->shutdown ();
    }
}

void
NotificationServiceMonitor_i::get_data (
   Monitor_Point_Registry* registry,
   const char* name,
   CosNotification::NotificationServiceMonitorControl::Data& data)
{
  // Get the statistic by name.
  TAO_Statistic* statistic =
    dynamic_cast<TAO_Statistic*> (registry->get (name));

  if (statistic == 0)
    {
      // At the time that the list of names were acquired, this
      // statistic was available.  However, we have failed to
      // retrieve it.  The only reason that could happen is if
      // some one removed the statistic from the registry.
      CosNotification::NotificationServiceMonitorControl::Numeric num;
      num.count = 0;
      num.average = 0;
      num.sum_of_squares = 0;
      num.minimum = 0;
      num.maximum = 0;
      num.last = 0;
      data.num (num);
    }
  else
    {
      // If it's not a counter, we need to make sure that we have
      // the most up-to-date information.  A counter will always have
      // the correct value.
      if (statistic->type () != TAO_Statistic::TS_COUNTER)
        {
          statistic->calculate ();
        }

      // Populate the data structure based on the type of statistic
      if (statistic->type () == TAO_Statistic::TS_LIST)
        {
          TAO_Statistic::List slist (statistic->get_list ());
          CORBA::ULong size = static_cast<CORBA::ULong> (slist.size ());
          Monitor::NameList list (size);
          list.length (size);

          for (CORBA::ULong i = 0; i < size; ++i)
            {
              list[i] = CORBA::string_dup (slist[i].c_str ());
            }

          data.list (list);
        }
      else
        {
          CosNotification::NotificationServiceMonitorControl::Numeric num;
          num.count = static_cast<CORBA::ULong> (statistic->count ());
          num.minimum = statistic->minimum_sample ();
          num.maximum = statistic->maximum_sample ();
          num.last = statistic->last_sample ();

          if (statistic->type() == TAO_Statistic::TS_COUNTER)
            {
              num.average = 0;
              num.sum_of_squares = 0;
            }
          else
            {
              num.average = statistic->average ();
              num.sum_of_squares = statistic->sum_of_squares ();
            }

          data.num (num);
        }
    }
}

void
NotificationServiceMonitor_i::get_invalid_names (
   Monitor_Point_Registry* registry,
   const Monitor::NameList& names,
   Monitor::NameList& invalid)
{
  invalid.length (0);

  CORBA::ULong ilength = 0;
  CORBA::ULong length  = names.length ();

  for (CORBA::ULong i = 0; i < length; ++i)
    {
      if (registry->get (names[i].in ()) == 0)
        {
          invalid.length (ilength + 1);
          invalid[ilength++] = CORBA::string_dup (names[i]);
        }
    }
}

TAO_END_VERSIONED_NAMESPACE_DECL

#endif /* ACE_HAS_MONITOR_FRAMEWORK==1 */

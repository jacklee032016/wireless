int osip_trace (char *srcfilename, int linenumber, as_trace_level_t level, FILE * f, char *chfr, ...)
{
	va_list ap;
#ifdef ENABLE_TRACE

#if !defined(WIN32) && !defined(SYSTEM_LOGGER_ENABLED)
	if (logfile == NULL && use_syslog == 0)
	{				/* user did not initialize logger.. */
		return 1;
	}
#endif

	if (f == NULL)
		f = logfile;

	if (tracing_table[level] == LOG_FALSE)
		return 0;

	VA_START (ap, chfr);


  if (f && use_syslog == 0)
    {
      if (level == OSIP_FATAL)
	fprintf (f, "| FATAL | <%s: %i> ", fi, li);
      else if (level == OSIP_BUG)
	fprintf (f, "|  BUG  | <%s: %i> ", fi, li);
      else if (level == OSIP_ERROR)
	fprintf (f, "| ERROR | <%s: %i> ", fi, li);
      else if (level == OSIP_WARNING)
	fprintf (f, "|WARNING| <%s: %i> ", fi, li);
      else if (level == OSIP_INFO1)
	fprintf (f, "| INFO1 | <%s: %i> ", fi, li);
      else if (level == OSIP_INFO2)
	fprintf (f, "| INFO2 | <%s: %i> ", fi, li);
      else if (level == OSIP_INFO3)
	fprintf (f, "| INFO3 | <%s: %i> ", fi, li);
      else if (level == OSIP_INFO4)
	fprintf (f, "| INFO4 | <%s: %i> ", fi, li);

      vfprintf (f, chfr, ap);

      fflush (f);
    }
#if defined (HAVE_SYSLOG_H)
  else if (use_syslog == 1)
    {
      char buffer[512];
      int in = 0;
      memset (buffer, 0, sizeof (buffer));
      if (level == OSIP_FATAL)
	in = snprintf (buffer, 511, "| FATAL | <%s: %i> ", fi, li);
      else if (level == OSIP_BUG)
	in = snprintf (buffer, 511, "|  BUG  | <%s: %i> ", fi, li);
      else if (level == OSIP_ERROR)
	in = snprintf (buffer, 511, "| ERROR | <%s: %i> ", fi, li);
      else if (level == OSIP_WARNING)
	in = snprintf (buffer, 511, "|WARNING| <%s: %i> ", fi, li);
      else if (level == OSIP_INFO1)
	in = snprintf (buffer, 511, "| INFO1 | <%s: %i> ", fi, li);
      else if (level == OSIP_INFO2)
	in = snprintf (buffer, 511, "| INFO2 | <%s: %i> ", fi, li);
      else if (level == OSIP_INFO3)
	in = snprintf (buffer, 511, "| INFO3 | <%s: %i> ", fi, li);
      else if (level == OSIP_INFO4)
	in = snprintf (buffer, 511, "| INFO4 | <%s: %i> ", fi, li);

      vsnprintf (buffer + in, 511 - in, chfr, ap);
      if (level == OSIP_FATAL)
	syslog (LOG_ERR, "%s", buffer);
      else if (level == OSIP_BUG)
	syslog (LOG_ERR, "%s", buffer);
      else if (level == OSIP_ERROR)
	syslog (LOG_ERR, "%s", buffer);
      else if (level == OSIP_WARNING)
	syslog (LOG_WARNING, "%s", buffer);
      else if (level == OSIP_INFO1)
	syslog (LOG_INFO, "%s", buffer);
      else if (level == OSIP_INFO2)
	syslog (LOG_INFO, "%s", buffer);
      else if (level == OSIP_INFO3)
	syslog (LOG_DEBUG, "%s", buffer);
      else if (level == OSIP_INFO4)
	syslog (LOG_DEBUG, "%s", buffer);
    }
#endif
#ifdef SYSTEM_LOGGER_ENABLED
  else
    {
      char buffer[512];
      int in = 0;
      memset (buffer, 0, sizeof (buffer));
      if (level == OSIP_FATAL)
	in = _snprintf (buffer, 511, "| FATAL | <%s: %i> ", fi, li);
      else if (level == OSIP_BUG)
	in = _snprintf (buffer, 511, "|  BUG  | <%s: %i> ", fi, li);
      else if (level == OSIP_ERROR)
	in = _snprintf (buffer, 511, "| ERROR | <%s: %i> ", fi, li);
      else if (level == OSIP_WARNING)
	in = _snprintf (buffer, 511, "|WARNING| <%s: %i> ", fi, li);
      else if (level == OSIP_INFO1)
	in = _snprintf (buffer, 511, "| INFO1 | <%s: %i> ", fi, li);
      else if (level == OSIP_INFO2)
	in = _snprintf (buffer, 511, "| INFO2 | <%s: %i> ", fi, li);
      else if (level == OSIP_INFO3)
	in = _snprintf (buffer, 511, "| INFO3 | <%s: %i> ", fi, li);
      else if (level == OSIP_INFO4)
	in = _snprintf (buffer, 511, "| INFO4 | <%s: %i> ", fi, li);

      _vsnprintf (buffer + in, 511 - in, chfr, ap);
      OutputDebugString (buffer);
    }
#endif

  va_end (ap);
#endif
  return 0;
}


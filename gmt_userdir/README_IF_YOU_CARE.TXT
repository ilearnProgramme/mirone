Files in this directory are meant to just allow a minimalist GMT
initialization, without giving any fatal errors.

In fact the idea is exactly that we can do a kind of blank initialization
in order to let Mirone GMT MEXs run undesturbed by the fact that GMT
might not be installed.

This works because the directory is set by the following line in mirone.m
set_gmt(['GMT_USERDIR=' home_dir fsep 'gmt_userdir'])
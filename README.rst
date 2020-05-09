####
aud2
####

Currently this is just trivial glue code between libav* and libasound.

The main differences between this repository and the lavf branch of
`buhman/aud`_:

- aud uses the libasound mmap interface--this is theoretically more efficient,
  but requires special/non-typical alsa configuration. aud2 uses
  ``snd_pcm_writei`` instead--this access method is compatible with the (widely
  configured) pulse sink

- aud uses several deprecated libavcodec and libavformat APIs

- aud generally has more unfixed bugs than aud2

.. _`buhman/aud`: https://github.com/buhman/aud

Usage:

.. code::

   $ ./aud2 ../celeste/*
   ../celeste/[Official] Celeste B-Sides - 04 - in love with a ghost - Golden Ridge (Golden Feather Mix)-AgDYV_IbPuo.webm
   selected stream: 0
   stream[0]: codec=opus
   samples: format=fltp rate=48000 channels=2
   .....................................................................................................................................................................................................................................................................................................
   End of file
   ../celeste/[Official] Celeste B-Sides - 05 - 2 Mello - Mirror Temple (Mirror Magic Mix)-iKnwVvXkWq0.webm
   selected stream: 0
   stream[0]: codec=opus
   samples: format=fltp rate=48000 channels=2
   ................................................

Ideas
=====

- use SND_PCM_ASYNC

- (global?) keyboard input (via libevent?)

  - seeking / skipping

- "stream" file paths via stdin, rather than argv

  - allow the "track" queue to change dynamically, rather than computed once
    when the player is started

- "port" to other platforms by providing aud2 as a minimal/tiny virtual machine
  image (e.g with VBoxManage convenience wrapper scripts for OSX)

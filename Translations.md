#summary Instructions for submitting translations for other languages
#labels Featured

# Translating text #
  1. Copy one of the existing language files in source/ngc/intl/ to the target language.
  1. Edit the file, replacing the strings with your translated text.
  1. Go to the _Issues_ tab, and submit a _New Issue_ (requires a Google account)
  1. Enter a **Summary** and **Description**.
  1. Click the _Attach a file_ link and attach your translated header file.
  1. Click _Submit Issue_ and wait.

# Compiling for a language #
  1. Open _Makefile.wii_ or _Makefile.gc_ in an editor.
  1. Search for the text **LANG**.  It should get you around line 24.
  1. Set the LANG variable to one of the supported languages listed, keeping it in all capital letters.
  1. Save and exit your editor.
  1. Compile the sources as usual.

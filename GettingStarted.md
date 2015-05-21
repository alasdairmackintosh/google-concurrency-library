#summary How to get started using or contributing to GCL

## Dependencies ##
  * svn This should be available on most machines/distributions
  * The standard make utility
  * A C++ compiler, ideally one like [gcc-4.6](http://gcc.gnu.org/gcc-4.6/cxx0x_status.html) that can be configured to conform to either the C++98 standard or the C++11 standard.
  * [Reitveld's upload.py](http://codereview.appspot.com/static/upload.py): For getting patches code reviewed. Put this in your path.

## How to build ##

You can build using the supplied `Makefile`.

```
$ mkdir gcl-whatever
$ cd gcl-whatever
$ svn checkout https://google-concurrency-library.googlecode.com/svn/ google-concurrency-library
# build libraries
$ make
# run tests
$ make test
```



## How to contribute ##

### Overview ###
Before committing changes to the central repository, you will need to have them reviewed. To do this, use http://codereview.appspot.com/use_uploadpy to
upload your patch to Reitveld so someone can review it. Running
`upload.py` with no arguments will create a new patch from your local
uncommitted changes. If you've already uploaded a patch, say to http://codereview.appspot.com/164050, then run `upload.py -i 164050` to update it.


### Getting changes approved ###
When using upload.py with no arguments, it won't
send email to let anyone know there's a review waiting for them. Go to
the codereview.appspot.com URL it gives you, click "Edit Issue" to
fill in the description and reviewers (make sure this includes
`google-concurrency-library@googlegroups.com` to archive all reviews),
and then click the "Publish+Mail Comments  ('m')" link to send the
review request. The first mail will automatically include the change's
description.

If you're not a committer, all you have to do is keep uploading new versions until the reviewers are happy, and then they'll commit it for you.

If you are a committer, once your change is reviewed, run `svn commit` to commit your changes to the main repository. If prompted for a password, you will need your Google Code password from http://code.google.com/hosting/settings.
### Editing wiki ###
If you don't want to edit the wiki from the browser, do this:
  * Checkout the wiki files from the repository. `svn checkout https://google-concurrency-library.googlecode.com/svn/wiki` (This will create a new directory with all wiki pages. Do it once)
  * Edit the wiki page using your favourite editor.
  * Commit the changes as described above.
  * `svn commit -m "Edited wiki" && hg push`
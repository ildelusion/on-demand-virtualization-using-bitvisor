# README #

This README would normally document whatever steps are necessary to get your application up and running.

### What is this repository for? ###

* Quick summary
  This SMM code is based on basic_smm, but Jaeseong remove the xen code and modify to execute smm in the kernel not xen.(2015-07-30)
* Version
* [Learn Markdown](https://bitbucket.org/tutorials/markdowndemo)

### How do I get set up? ###

* --- Summary of set up ---
*  $ make clean; make
*  $ cd ./tools
*  $ make clean; make
*  $ sudo modprobe msr
*  If you don't have modprobe, $ sudo apt-get install msr-tools
*  $ sudo ./copy_smm
*  $ sudo ./invoke_smi


### Contribution guidelines ###

* Writing tests
* Code review
* Other guidelines

### Who do I talk to? ###

* Repo owner or admin
* Other community or team contact
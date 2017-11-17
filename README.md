# libmigrate

To compile the static library alone into the *lib/* directory, you can simply run:

```
make
```

To compile the static library and the tests, you can run the following command:

```
make tests
```

If you encounter an error that says `No such file or directory` when running the tests or an application that is linked against the library, you can either run the application as root (`sudo /path/to/app`) or ensure that the user that the application is running under has read and write permissions to the */var/migrated/* directory.

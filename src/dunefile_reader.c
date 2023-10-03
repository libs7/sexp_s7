#include <assert.h>
#include <ctype.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <unistd.h>

#include "libs7.h"
#include "dunefile_reader.h"

const char *dunefile_to_string(s7_scheme *s7, const char *dunefile_name)
{
    TRACE_ENTRY;
#if defined(DEBUG_fastbuild)
    LOG_TRACE(1, "dunefile: %s", dunefile_name);
                  //utstring_body(dunefile_name));
    s7_pointer cip = s7_current_input_port(s7);
    TRACE_S7_DUMP(1, "cip", cip);
#endif
    /* core/dune file size: 45572 */
    // 2K

    //FIXME: malloc
/* #define DUNE_BUFSZ 131072 */
/*     /\* static char inbuf[DUNE_BUFSZ]; *\/ */
/*     /\* memset(inbuf, '\0', DUNE_BUFSZ); *\/ */
/*     static char outbuf[DUNE_BUFSZ + 20]; */
/*     memset(outbuf, '\0', DUNE_BUFSZ); */

    size_t file_size;
    char *inbuf = NULL;
    struct stat stbuf;
    int fd;
    FILE *instream = NULL;

    errno = 0;
    fd = open(dunefile_name, O_RDONLY);
    if (fd == -1) {
        /* Handle error */
        LOG_ERROR(0, "fd open error: %s", dunefile_name);
        LOG_TRACE(1, "cwd: %s", getcwd(NULL, 0));
        s7_error(s7, s7_make_symbol(s7, "fd-open-error"),
                 s7_list(s7, 3,
                         s7_make_string(s7, "fd open error: ~A, ~A"),
                         s7_make_string(s7, dunefile_name),
                         s7_make_string(s7, strerror(errno))));
    }

    if ((fstat(fd, &stbuf) != 0) || (!S_ISREG(stbuf.st_mode))) {
        /* Handle error */
        LOG_ERROR(0, "fstat error", "");
        goto cleanup;
    }

    file_size = stbuf.st_size;
#if defined(DEBUG_fastbuild)
    LOG_DEBUG(1, "filesize: %d", file_size);
#endif

    inbuf = (char*)calloc(file_size, sizeof(char));
    if (inbuf == NULL) {
        /* Handle error */
        LOG_ERROR(0, "malloc file_size fail", "");
        goto cleanup;
    }

    /* FIXME: what about e.g. unicode in string literals? */
    errno = 0;
    instream = fdopen(fd, "r");
    if (instream == NULL) {
        /* Handle error */
        LOG_ERROR(0, "fdopen failure: %s", dunefile_name);
        /* printf(RED "ERROR" CRESET "fdopen failure: %s\n", */
        /*        dunefile_name); */
               /* utstring_body(dunefile_name)); */
        perror(NULL);
        close(fd);
        goto cleanup;
    } else {
#if defined(DEBUG_fastbuild)
        LOG_DEBUG(1, "fdopened %s", dunefile_name);
        /* utstring_body(dunefile_name)); */
#endif
    }

    // now read the entire file
    size_t read_ct = fread(inbuf, 1, file_size, instream);
#if defined(DEBUG_fastbuild)
    LOG_DEBUG(1, "read_ct: %d", read_ct);
    LOG_DEBUG(1, "readed txt: %s", (char*)inbuf);
#endif
    if (read_ct != file_size) {
        if (ferror(instream) != 0) {
            /* printf(RED "ERROR" CRESET "fread error 2 for %s\n", */
            /*        dunefile_name); */
            /* utstring_body(dunefile_name)); */
            LOG_ERROR(0, "fread error 2 for %s\n",
                      dunefile_name);
            /* utstring_body(dunefile_name)); */
            exit(EXIT_FAILURE); //FIXME: exit gracefully
        } else {
            if (feof(instream) == 0) {
                /* printf(RED "ERROR" CRESET "fread error 3 for %s\n", */
                /*        dunefile_name); */
                /* utstring_body(dunefile_name)); */
                LOG_ERROR(0, "fread error 3 for %s\n",
                          dunefile_name);
                /* utstring_body(dunefile_name)); */
                exit(EXIT_FAILURE); //FIXME: exit gracefully
            } else {
                //FIXME
                LOG_ERROR(0, "WTF????????????????", "");
                goto cleanup;
            }
        }
    } else {
        close(fd);
        fclose(instream);
    }

    inbuf[read_ct + 1] = '\0';
    // allocate twice (?) what we need
    uint64_t outFileSizeCounter = file_size * 3;
    errno = 0;
    fflush(NULL);
    static char outbuf[320000];
    /* char *outbuf = NULL; */
    /* outbuf = (char*)malloc(outFileSizeCounter); */
    /* outbuf = (char*)calloc(outFileSizeCounter, sizeof(char)); */
    /* fprintf(stderr, "XXXXXXXXXXXXXXXX"); */
    /* fflush(NULL); */
    /* if (outbuf == NULL) { */
    /*     LOG_ERROR(0, "calloc fail: %s", strerror(errno)); */
    /*     goto cleanup; */
    /* } else { */
    /*     LOG_INFO(0, "calloc success"); */
    /* } */
    memset((char*)outbuf, '\0', outFileSizeCounter);

    // FIXME: loop over the entire inbuf char by char, testing for
    // . or "\|
    char *inptr = (char*)inbuf;
    /* LOG_DEBUG(1, "INPTR str: %s", inptr); */
    char *outptr = (char*)outbuf;
    /* char *cursor = inptr; */

    bool eol_string = false;
    while (*inptr) {
        if (*inptr == '.') {
            if ((*(inptr+1) == ')')
                && isspace(*(inptr-1))){
                /* LOG_DEBUG(1, "FOUND DOT: %s", inptr); */
                *outptr++ = *inptr++;
                *outptr++ = '/';
                continue;
            }
        }
        if (*inptr == '"') {
            if (*(inptr+1) == '\\') {
                if (*(inptr+2) == '|') {
                    /* LOG_DEBUG(1, "FOUND EOL Q"); */
                    *outptr++ = *inptr++; // copy '"'
                    inptr += 2; // point to char after '"\|'
                    eol_string = true;
                    while (eol_string) {
                        if (*inptr == '\0') {
                            *outptr = *inptr;
                            eol_string = false;
                        }
                        if (*inptr == '\n') {
                            /* LOG_DEBUG(1, "hit eolstring newline"); */
                            // check to see if next line starts with "\|
                            char *tmp = inptr + 1;
                            while (isspace(*tmp)) {tmp++;}
                            /* LOG_DEBUG(1, "skipped to: %s", tmp); */
                            if (*(tmp) == '"') {
                                if (*(tmp+1) == '\\') {
                                    if (*(tmp+2) == '|') {
                                        // preserve \n
                                        *outptr++ = *inptr;
                                        /* *outptr++ = '\\'; */
                                        /* *outptr++ = 'n'; */
                                        inptr = tmp + 3;
                                        /* LOG_DEBUG(1, "resuming at %s", inptr); */
                                        continue;
                                    }
                                }
                            }
                            *outptr++ = '"';
                            inptr++; // omit \n
                            eol_string = false;
                        } else {
                            *outptr++ = *inptr++;
                        }
                    }
                }
            }
        }
        *outptr++ = *inptr++;
    }

/*     inptr = (char*)inbuf; */
/*     while (true) { */
/*         cursor = strstr(inptr, ".)"); */

/* /\* https://stackoverflow.com/questions/54592366/replacing-one-character-in-a-string-with-multiple-characters-in-c *\/ */

/*         if (cursor == NULL) { */
/* /\* #if defined(DEBUG_fastbuild) *\/ */
/* /\*             if (mibl_debug) LOG_DEBUG(1, "remainder: '%s'", inptr); *\/ */
/* /\* #endif *\/ */
/*             size_t ct = strlcpy(outptr, (const char*)inptr, file_size); // strlen(outptr)); */
/*             (void)ct;           /\* prevent -Wunused-variable *\/ */
/* /\* #if defined(DEBUG_fastbuild) *\/ */
/* /\*             if (mibl_debug) LOG_DEBUG(1, "concatenated: '%s'", outptr); *\/ */
/* /\* #endif *\/ */
/*             break; */
/*         } else { */
/* #if defined(DEBUG_fastbuild) */
/*             LOG_ERROR(0, "FOUND and fixing \".)\" at pos: %d", cursor - inbuf); */
/* #endif */
/*             size_t ct = strlcpy(outptr, (const char*)inptr, cursor - inptr); */
/* #if defined(DEBUG_fastbuild) */
/*             LOG_DEBUG(1, "copied %d chars", ct); */
/*             /\* LOG_DEBUG(1, "to buf: '%s'", outptr); *\/ */
/* #endif */
/*             /\* if (ct >= DUNE_BUFSZ) { *\/ */
/*             if (ct >= outFileSizeCounter) { */
/*                 printf("output string has been truncated!\n"); */
/*             } */
/*             outptr = outptr + (cursor - inptr) - 1; */
/*             outptr[cursor - inptr] = '\0'; */
/*             //FIXME: use memcpy */
/*             ct = strlcat(outptr, " ./", outFileSizeCounter); // DUNE_BUFSZ); */
/*             outptr += 3; */

/*             inptr = inptr + (cursor - inptr) + 1; */
/*             /\* printf(GRN "inptr:\n" CRESET " %s\n", inptr); *\/ */

/*             if (ct >= outFileSizeCounter) { // DUNE_BUFSZ) { */
/*                 LOG_ERROR(0, "write count exceeded output bufsz\n"); */
/*                 /\* printf(RED "ERROR" CRESET "write count exceeded output bufsz\n"); *\/ */
/*                 free(inbuf); */
/*                 exit(EXIT_FAILURE); */
/*                 // output string has been truncated */
/*             } */
/*         } */
/*     } */
    /* free(inbuf); */

    /* char *tmp = strndup((char*) outbuf, strlen((char*)outbuf)); */
    /* LOG_DEBUG(1, "x AAAAAAAAAAAAAAAA9"); */
    /* free(outbuf); */
    return (char*)outbuf;

cleanup:
    //FIXME
    if (instream != NULL)
    {
        fclose(instream);
        close(fd);
    }
    if (inbuf != NULL) free(inbuf);
    /* if (outbuf != NULL) free(outbuf); */
    return NULL;
}

char *read_dunefile(const char *dunefile_name)
{
    TRACE_ENTRY;
    /* log_debug("df: %s", dunefile_name); */
    TRACE_LOG("dunefile: %s", dunefile_name);

    size_t file_size;
    char *inbuf = NULL;
    struct stat stbuf;
    int fd;
    FILE *instream = NULL;

    errno = 0;
    fd = open(dunefile_name, O_RDONLY);
    if (fd == -1) {
        /* Handle error */
        LOG_ERROR(0, "fd open error: %s", dunefile_name);
        LOG_TRACE(1, "cwd: %s", getcwd(NULL, 0));
        /* s7_error(s7, s7_make_symbol(s7, "fd-open-error"), */
        /*          s7_list(s7, 3, */
        /*                  s7_make_string(s7, "fd open error: ~A, ~A"), */
        /*                  s7_make_string(s7, dunefile_name), */
        /*                  s7_make_string(s7, strerror(errno)))); */
    }

    if ((fstat(fd, &stbuf) != 0) || (!S_ISREG(stbuf.st_mode))) {
        /* Handle error */
        LOG_ERROR(0, "fstat error", "");
        goto cleanup;
    }

    file_size = stbuf.st_size;
#if defined(DEBUG_fastbuild)
    LOG_DEBUG(1, "filesize: %d", file_size);
#endif

    inbuf = (char*)calloc(file_size, sizeof(char));
    if (inbuf == NULL) {
        /* Handle error */
        LOG_ERROR(0, "malloc file_size fail", "");
        goto cleanup;
    }

    /* FIXME: what about e.g. unicode in string literals? */
    errno = 0;
    instream = fdopen(fd, "r");
    if (instream == NULL) {
        /* Handle error */
        LOG_ERROR(0, "fdopen failure: %s", dunefile_name);
        /* printf(RED "ERROR" CRESET "fdopen failure: %s\n", */
        /*        dunefile_name); */
               /* utstring_body(dunefile_name)); */
        perror(NULL);
        close(fd);
        goto cleanup;
    } else {
#if defined(DEBUG_fastbuild)
        LOG_DEBUG(1, "fdopened %s", dunefile_name);
        /* utstring_body(dunefile_name)); */
#endif
    }

    // now read the entire file
    size_t read_ct = fread(inbuf, 1, file_size, instream);
#if defined(DEBUG_fastbuild)
    LOG_DEBUG(1, "read_ct: %d", read_ct);
    LOG_DEBUG(1, "readed txt: %s", (char*)inbuf);
#endif
    if (read_ct != file_size) {
        if (ferror(instream) != 0) {
            /* printf(RED "ERROR" CRESET "fread error 2 for %s\n", */
            /*        dunefile_name); */
            /* utstring_body(dunefile_name)); */
            LOG_ERROR(0, "fread error 2 for %s\n",
                      dunefile_name);
            /* utstring_body(dunefile_name)); */
            exit(EXIT_FAILURE); //FIXME: exit gracefully
        } else {
            if (feof(instream) == 0) {
                /* printf(RED "ERROR" CRESET "fread error 3 for %s\n", */
                /*        dunefile_name); */
                /* utstring_body(dunefile_name)); */
                LOG_ERROR(0, "fread error 3 for %s\n",
                          dunefile_name);
                /* utstring_body(dunefile_name)); */
                exit(EXIT_FAILURE); //FIXME: exit gracefully
            } else {
                //FIXME
                LOG_ERROR(0, "WTF????????????????", "");
                goto cleanup;
            }
        }
    } else {
        close(fd);
        fclose(instream);
    }

    inbuf[read_ct + 1] = '\0';
    // allocate twice (?) what we need
    uint64_t outFileSizeCounter = file_size * 3;
    errno = 0;
    fflush(NULL);
    static char outbuf[320000];
    /* char *outbuf = NULL; */
    /* outbuf = (char*)malloc(outFileSizeCounter); */
    /* outbuf = (char*)calloc(outFileSizeCounter, sizeof(char)); */
    /* fprintf(stderr, "XXXXXXXXXXXXXXXX"); */
    /* fflush(NULL); */
    /* if (outbuf == NULL) { */
    /*     LOG_ERROR(0, "calloc fail: %s", strerror(errno)); */
    /*     goto cleanup; */
    /* } else { */
    /*     LOG_INFO(0, "calloc success"); */
    /* } */
    memset((char*)outbuf, '\0', outFileSizeCounter);

    // FIXME: loop over the entire inbuf char by char, testing for
    // . or "\|
    char *inptr = (char*)inbuf;
    /* LOG_DEBUG(1, "INPTR str: %s", inptr); */
    char *outptr = (char*)outbuf;
    /* char *cursor = inptr; */

    bool eol_string = false;
    while (*inptr) {
        if (*inptr == '.') {
            if ((*(inptr+1) == ')')
                && isspace(*(inptr-1))){
                /* LOG_DEBUG(1, "FOUND DOT: %s", inptr); */
                *outptr++ = *inptr++;
                *outptr++ = '/';
                continue;
            }
        }
        if (*inptr == '"') {
            if (*(inptr+1) == '\\') {
                if (*(inptr+2) == '|') {
                    /* LOG_DEBUG(1, "FOUND EOL Q"); */
                    *outptr++ = *inptr++; // copy '"'
                    inptr += 2; // point to char after '"\|'
                    eol_string = true;
                    while (eol_string) {
                        if (*inptr == '\0') {
                            *outptr = *inptr;
                            eol_string = false;
                        }
                        if (*inptr == '\n') {
                            /* LOG_DEBUG(1, "hit eolstring newline"); */
                            // check to see if next line starts with "\|
                            char *tmp = inptr + 1;
                            while (isspace(*tmp)) {tmp++;}
                            /* LOG_DEBUG(1, "skipped to: %s", tmp); */
                            if (*(tmp) == '"') {
                                if (*(tmp+1) == '\\') {
                                    if (*(tmp+2) == '|') {
                                        // preserve \n
                                        *outptr++ = *inptr;
                                        /* *outptr++ = '\\'; */
                                        /* *outptr++ = 'n'; */
                                        inptr = tmp + 3;
                                        /* LOG_DEBUG(1, "resuming at %s", inptr); */
                                        continue;
                                    }
                                }
                            }
                            *outptr++ = '"';
                            inptr++; // omit \n
                            eol_string = false;
                        } else {
                            *outptr++ = *inptr++;
                        }
                    }
                }
            }
        }
        *outptr++ = *inptr++;
    }

/*     inptr = (char*)inbuf; */
/*     while (true) { */
/*         cursor = strstr(inptr, ".)"); */

/* /\* https://stackoverflow.com/questions/54592366/replacing-one-character-in-a-string-with-multiple-characters-in-c *\/ */

/*         if (cursor == NULL) { */
/* /\* #if defined(DEBUG_fastbuild) *\/ */
/* /\*             if (mibl_debug) LOG_DEBUG(1, "remainder: '%s'", inptr); *\/ */
/* /\* #endif *\/ */
/*             size_t ct = strlcpy(outptr, (const char*)inptr, file_size); // strlen(outptr)); */
/*             (void)ct;           /\* prevent -Wunused-variable *\/ */
/* /\* #if defined(DEBUG_fastbuild) *\/ */
/* /\*             if (mibl_debug) LOG_DEBUG(1, "concatenated: '%s'", outptr); *\/ */
/* /\* #endif *\/ */
/*             break; */
/*         } else { */
/* #if defined(DEBUG_fastbuild) */
/*             LOG_ERROR(0, "FOUND and fixing \".)\" at pos: %d", cursor - inbuf); */
/* #endif */
/*             size_t ct = strlcpy(outptr, (const char*)inptr, cursor - inptr); */
/* #if defined(DEBUG_fastbuild) */
/*             LOG_DEBUG(1, "copied %d chars", ct); */
/*             /\* LOG_DEBUG(1, "to buf: '%s'", outptr); *\/ */
/* #endif */
/*             /\* if (ct >= DUNE_BUFSZ) { *\/ */
/*             if (ct >= outFileSizeCounter) { */
/*                 printf("output string has been truncated!\n"); */
/*             } */
/*             outptr = outptr + (cursor - inptr) - 1; */
/*             outptr[cursor - inptr] = '\0'; */
/*             //FIXME: use memcpy */
/*             ct = strlcat(outptr, " ./", outFileSizeCounter); // DUNE_BUFSZ); */
/*             outptr += 3; */

/*             inptr = inptr + (cursor - inptr) + 1; */
/*             /\* printf(GRN "inptr:\n" CRESET " %s\n", inptr); *\/ */

/*             if (ct >= outFileSizeCounter) { // DUNE_BUFSZ) { */
/*                 LOG_ERROR(0, "write count exceeded output bufsz\n"); */
/*                 /\* printf(RED "ERROR" CRESET "write count exceeded output bufsz\n"); *\/ */
/*                 free(inbuf); */
/*                 exit(EXIT_FAILURE); */
/*                 // output string has been truncated */
/*             } */
/*         } */
/*     } */
    /* free(inbuf); */

    /* char *tmp = strndup((char*) outbuf, strlen((char*)outbuf)); */
    /* LOG_DEBUG(1, "x AAAAAAAAAAAAAAAA9"); */
    /* free(outbuf); */
    return (char*)outbuf;

cleanup:
    //FIXME
    if (instream != NULL)
    {
        fclose(instream);
        close(fd);
    }
    if (inbuf != NULL) free(inbuf);
    /* if (outbuf != NULL) free(outbuf); */
    return NULL;
}


/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0.  If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright 1997 - July 2008 CWI, August 2008 - 2016 MonetDB B.V.
 */

/*
 * @a M. L. Kersten, P. Boncz, N. Nes
 *
 * @* Database Storage Management
 * Contains routines for writing and reading GDK data to and from
 * disk.  This section contains the primitives to manage the
 * disk-based images of the BATs. It relies on the existence of a UNIX
 * file system, including memory mapped files. Solaris and IRIX have
 * different implementations of madvise().
 *
 * The current version assumes that all BATs are stored on a single
 * disk partition. This simplistic assumption should be replaced in
 * the near future by a multi-volume version. The intention is to use
 * several BAT home locations.  The files should be owned by the
 * database server. Otherwise, IO operations are likely to fail. This
 * is accomplished by setting the GID and UID upon system start.
 */
#include "monetdb_config.h"
#include "gdk.h"
#include "gdk_private.h"
#include <stdlib.h>
#include "gdk_storage.h"
#include "mutils.h"
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

/* GDKfilepath returns a newly allocated string containing the path
 * name of a database farm.
 * The arguments are the farmID or -1, the name of a subdirectory
 * within the farm (i.e., something like BATDIR or BAKDIR -- see
 * gdk.h) or NULL, the name of a BAT (i.e. the name that is stored in
 * BBP.dir -- something like 07/714), and finally the file extension.
 *
 * If farmid is >= 0, GDKfilepath returns the complete path to the
 * specified farm concatenated with the other arguments with
 * appropriate separators.  If farmid is -1, it returns the
 * concatenation of its other arguments (in this case, the result
 * cannot be used to access a file directly -- the farm needs to be
 * prepended in some other place). */
char *
GDKfilepath(int farmid, const char *dir, const char *name, const char *ext)
{
	char sep[2];
	size_t pathlen;
	char *path;

	assert(dir == NULL || *dir != DIR_SEP);
	assert(farmid == NOFARM ||
	       (farmid >= 0 && farmid < MAXFARMS && BBPfarms[farmid].dirname));
	if (MT_path_absolute(name)) {
		GDKerror("GDKfilepath: name should not be absolute\n");
		return NULL;
	}
	if (dir && *dir == DIR_SEP)
		dir++;
	if (dir == NULL || dir[0] == 0 || dir[strlen(dir) - 1] == DIR_SEP) {
		sep[0] = 0;
	} else {
		sep[0] = DIR_SEP;
		sep[1] = 0;
	}
	pathlen = (farmid == NOFARM ? 0 : strlen(BBPfarms[farmid].dirname) + 1) +
		(dir ? strlen(dir) : 0) + strlen(sep) + strlen(name) +
		(ext ? strlen(ext) + 1 : 0) + 1;
	path = GDKmalloc(pathlen);
	if (path == NULL)
		return NULL;
	if (farmid == NOFARM) {
		snprintf(path, pathlen, "%s%s%s%s%s",
			 dir ? dir : "", sep, name,
			 ext ? "." : "", ext ? ext : "");
	} else {
		snprintf(path, pathlen, "%s%c%s%s%s%s%s",
			 BBPfarms[farmid].dirname, DIR_SEP,
			 dir ? dir : "", sep, name,
			 ext ? "." : "", ext ? ext : "");
	}
	return path;
}

/* make sure the parent directory of DIR exists (the argument itself
 * is usually a file that is to be created) */
gdk_return
GDKcreatedir(const char *dir)
{
	char path[PATHLENGTH];
	char *r;
	DIR *dirp;

	IODEBUG fprintf(stderr, "#GDKcreatedir(%s)\n", dir);
	assert(MT_path_absolute(dir));
	if (strlen(dir) >= PATHLENGTH) {
		GDKerror("GDKcreatedir: directory name too long\n");
		return GDK_FAIL;
	}
	strcpy(path, dir);	/* we know this fits (see above) */
	/* skip initial /, if any */
	for (r = strchr(path + 1, DIR_SEP); r; r = strchr(r, DIR_SEP)) {
		*r = 0;
		if (
#ifdef WIN32
			strlen(path) > 3 &&
#endif
			mkdir(path, 0755) < 0) {
			if (errno != EEXIST) {
				GDKsyserror("GDKcreatedir: cannot create directory %s\n", path);
				IODEBUG fprintf(stderr, "#GDKcreatedir: mkdir(%s) failed\n", path);
				return GDK_FAIL;
			}
			if ((dirp = opendir(path)) == NULL) {
				GDKerror("GDKcreatedir: %s not a directory\n", path);
				IODEBUG fprintf(stderr, "#GDKcreatedir: opendir(%s) failed\n", path);
				return GDK_FAIL;
			}
			/* it's a directory, we can continue */
			closedir(dirp);
		}
		*r++ = DIR_SEP;
	}
	return GDK_SUCCEED;
}

/* remove the directory DIRNAME with its file contents; does not
 * recurse into subdirectories */
gdk_return
GDKremovedir(int farmid, const char *dirname)
{
	str dirnamestr;
	DIR *dirp;
	char *path;
	struct dirent *dent;
	int ret;

	if ((dirnamestr = GDKfilepath(farmid, NULL, dirname, NULL)) == NULL)
		return GDK_FAIL;

	IODEBUG fprintf(stderr, "#GDKremovedir(%s)\n", dirnamestr);

	if ((dirp = opendir(dirnamestr)) == NULL) {
		GDKfree(dirnamestr);
		return GDK_SUCCEED;
	}
	while ((dent = readdir(dirp)) != NULL) {
		if (dent->d_name[0] == '.' &&
		    (dent->d_name[1] == 0 ||
		     (dent->d_name[1] == '.' && dent->d_name[2] == 0))) {
			/* skip . and .. */
			continue;
		}
		path = GDKfilepath(farmid, dirname, dent->d_name, NULL);
		ret = unlink(path);
		IODEBUG fprintf(stderr, "#unlink %s = %d\n", path, ret);
		GDKfree(path);
	}
	closedir(dirp);
	ret = rmdir(dirnamestr);
	if (ret < 0)
		GDKsyserror("GDKremovedir: rmdir(%s) failed.\n", dirnamestr);
	IODEBUG fprintf(stderr, "#rmdir %s = %d\n", dirnamestr, ret);
	GDKfree(dirnamestr);
	return ret ? GDK_FAIL : GDK_SUCCEED;
}

#define _FUNBUF		0x040000
#define _FWRTHR		0x080000
#define _FRDSEQ		0x100000

/* open a file and return its file descriptor; the file is specified
 * using farmid, name and extension; if opening for writing, we create
 * the parent directory if necessary */
int
GDKfdlocate(int farmid, const char *nme, const char *mode, const char *extension)
{
	char *path;
	int fd, flags = 0;

	if (nme == NULL || *nme == 0)
		return -1;

	path = GDKfilepath(farmid, BATDIR, nme, extension);
	if (path == NULL)
		return -1;

	if (*mode == 'm') {	/* file open for mmap? */
		mode++;
#ifdef _CYGNUS_H_
	} else {
		flags = _FRDSEQ;	/* WIN32 CreateFile(FILE_FLAG_SEQUENTIAL_SCAN) */
#endif
	}

	if (strchr(mode, 'w')) {
		flags |= O_WRONLY | O_CREAT;
	} else if (!strchr(mode, '+')) {
		flags |= O_RDONLY;
	} else {
		flags |= O_RDWR;
	}
#ifdef WIN32
	flags |= strchr(mode, 'b') ? O_BINARY : O_TEXT;
#endif
	fd = open(path, flags, MONETDB_MODE);
	if (fd < 0 && *mode == 'w') {
		/* try to create the directory, in case that was the problem */
		if (GDKcreatedir(path) == GDK_SUCCEED) {
			fd = open(path, flags, MONETDB_MODE);
			if (fd < 0)
				GDKsyserror("GDKfdlocate: cannot open file %s\n", path);
		}
	}
	/* don't generate error if we can't open a file for reading */
	GDKfree(path);
	return fd;
}

/* like GDKfdlocate, except return a FILE pointer */
FILE *
GDKfilelocate(int farmid, const char *nme, const char *mode, const char *extension)
{
	int fd;
	FILE *f;

	if ((fd = GDKfdlocate(farmid, nme, mode, extension)) < 0)
		return NULL;
	if (*mode == 'm')
		mode++;
	if ((f = fdopen(fd, mode)) == NULL) {
		GDKsyserror("GDKfilelocate: cannot fdopen file\n");
		close(fd);
		return NULL;
	}
	return f;
}

FILE *
GDKfileopen(int farmid, const char * dir, const char *name, const char *extension, const char *mode)
{
	char *path;

	/* if name is null, try to get one from dir (in case it was a path) */
	path = GDKfilepath(farmid, dir, name, extension);

	if (path != NULL) {
		FILE *f;
		IODEBUG fprintf(stderr, "#GDKfileopen(%s)\n", path);
		f = fopen(path, mode);
		GDKfree(path);
		return f;
	}
	return NULL;
}

/* unlink the file */
gdk_return
GDKunlink(int farmid, const char *dir, const char *nme, const char *ext)
{
	if (nme && *nme) {
		char *path;

		path = GDKfilepath(farmid, dir, nme, ext);
		/* if file already doesn't exist, we don't care */
		if (unlink(path) == -1 && errno != ENOENT) {
			GDKsyserror("GDKunlink(%s)\n", path);
			IODEBUG fprintf(stderr, "#unlink %s = -1\n", path);
			GDKfree(path);
			return GDK_FAIL;
		}
		GDKfree(path);
		return GDK_SUCCEED;
	}
	return GDK_FAIL;
}

/*
 * A move routine is overloaded to deal with extensions.
 */
gdk_return
GDKmove(int farmid, const char *dir1, const char *nme1, const char *ext1, const char *dir2, const char *nme2, const char *ext2)
{
	char *path1;
	char *path2;
	int ret, t0 = 0;

	IODEBUG t0 = GDKms();

	if ((nme1 == NULL) || (*nme1 == 0)) {
		errno = EFAULT;
		return GDK_FAIL;
	}
	path1 = GDKfilepath(farmid, dir1, nme1, ext1);
	path2 = GDKfilepath(farmid, dir2, nme2, ext2);
	if (path1 && path2) {
		ret = rename(path1, path2);
		if (ret < 0)
			GDKsyserror("GDKmove: cannot rename %s to %s\n", path1, path2);

		IODEBUG fprintf(stderr, "#move %s %s = %d (%dms)\n", path1, path2, ret, GDKms() - t0);
	} else {
		ret = -1;
	}
	GDKfree(path1);
	GDKfree(path2);
	return ret < 0 ? GDK_FAIL : GDK_SUCCEED;
}

gdk_return
GDKextendf(int fd, size_t size, const char *fn)
{
	struct stat stb;
	int rt = 0;
	int t0 = 0;

	if (fstat(fd, &stb) < 0) {
		/* shouldn't happen */
		GDKsyserror("GDKextendf: fstat unexpectedly failed\n");
		return GDK_FAIL;
	}
	/* if necessary, extend the underlying file */
	IODEBUG t0 = GDKms();
	if (stb.st_size < (off_t) size) {
#ifdef HAVE_FALLOCATE
		if ((rt = fallocate(fd, 0, stb.st_size, (off_t) size - stb.st_size)) < 0 &&
		    errno == EOPNOTSUPP)
			/* on Linux, posix_fallocate uses a slow
			 * method to allocate blocks if the underlying
			 * file system doesn't support the operation,
			 * so use fallocate instead and just resize
			 * the file if it fails */
#else
#ifdef HAVE_POSIX_FALLOCATE
		/* posix_fallocate returns error number on failure,
		 * not -1 :-( */
		if ((rt = posix_fallocate(fd, stb.st_size, (off_t) size - stb.st_size)) == EINVAL)
			/* on Solaris/OpenIndiana, this may mean that
			 * the underlying file system doesn't support
			 * the operation, so just resize the file */
#endif
#endif
		/* we get here when (posix_)fallocate fails because it
		 * is not supported on the file system, or if neither
		 * function exists */
		rt = ftruncate(fd, (off_t) size);
		if (rt != 0) {
			/* extending failed, try to reduce file size
			 * back to original */
			int err = errno;
			if (ftruncate(fd, stb.st_size))
				perror("ftruncate");
			errno = err; /* restore for error message */
			GDKsyserror("GDKextendf: could not extend file\n");
		}
	}
	IODEBUG fprintf(stderr, "#GDKextend %s " SZFMT " -> " SZFMT " %dms%s\n",
			fn, (size_t) stb.st_size, size,
			GDKms() - t0, rt != 0 ? " (failed)" : "");
	/* posix_fallocate returns != 0 on failure, fallocate and
	 * ftruncate return -1 on failure, but all three return 0 on
	 * success */
	return rt != 0 ? GDK_FAIL : GDK_SUCCEED;
}

gdk_return
GDKextend(const char *fn, size_t size)
{
	int fd, flags = O_RDWR;
	gdk_return rt = GDK_FAIL;

#ifdef O_BINARY
	/* On Windows, open() fails if the file is bigger than 2^32
	 * bytes without O_BINARY. */
	flags |= O_BINARY;
#endif
	if ((fd = open(fn, flags)) >= 0) {
		rt = GDKextendf(fd, size, fn);
		close(fd);
	} else {
		GDKsyserror("GDKextend: cannot open file %s\n", fn);
	}
	return rt;
}

/*
 * @+ Save and load.
 * The BAT is saved on disk in several files. The extension DESC
 * denotes the descriptor, BUNs the bun heap, and HHEAP and THEAP the
 * other heaps. The storage mechanism off a file can be memory mapped
 * (STORE_MMAP) or malloced (STORE_MEM).
 *
 * These modes indicates the disk-layout and the intended mapping.
 * The primary concern here is to handle STORE_MMAP and STORE_MEM.
 */
gdk_return
GDKsave(int farmid, const char *nme, const char *ext, void *buf, size_t size, storage_t mode, int dosync)
{
	int err = 0;

	IODEBUG fprintf(stderr, "#GDKsave: name=%s, ext=%s, mode %d, dosync=%d\n", nme, ext ? ext : "", (int) mode, dosync);

	if (mode == STORE_MMAP) {
		if (dosync && size && MT_msync(buf, size) < 0)
			err = -1;
		if (err)
			GDKsyserror("GDKsave: error on: name=%s, ext=%s, "
				    "mode=%d\n", nme, ext ? ext : "",
				    (int) mode);
		IODEBUG fprintf(stderr,
				"#MT_msync(buf " PTRFMT ", size " SZFMT
				") = %d\n",
				PTRFMTCAST buf, size, err);
	} else {
		int fd;

		if ((fd = GDKfdlocate(farmid, nme, "wb", ext)) >= 0) {
			/* write() on 64-bits Redhat for IA64 returns
			 * 32-bits signed result (= OS BUG)! write()
			 * on Windows only takes unsigned int as
			 * size */
			while (size > 0) {
				/* circumvent problems by writing huge
				 * buffers in chunks <= 1GiB */
				ssize_t ret;

				ret = write(fd, buf,
					    (unsigned) MIN(1 << 30, size));
				if (ret < 0) {
					err = -1;
					GDKsyserror("GDKsave: error " SSZFMT
						    " on: name=%s, ext=%s, "
						    "mode=%d\n", ret, nme,
						    ext ? ext : "", (int) mode);
					break;
				}
				size -= ret;
				buf = (void *) ((char *) buf + ret);
				IODEBUG fprintf(stderr,
						"#write(fd %d, buf " PTRFMT
						", size %u) = " SSZFMT "\n",
						fd, PTRFMTCAST buf,
						(unsigned) MIN(1 << 30, size),
						ret);
			}
			if (dosync && !(GDKdebug & FORCEMITOMASK) &&
#if defined(NATIVE_WIN32)
			    _commit(fd) < 0
#elif defined(HAVE_FDATASYNC)
			    fdatasync(fd) < 0
#elif defined(HAVE_FSYNC)
			    fsync(fd) < 0
#else
			    0
#endif
				) {
				GDKsyserror("GDKsave: error on: name=%s, "
					    "ext=%s, mode=%d\n", nme,
					    ext ? ext : "", (int) mode);
				err = -1;
			}
			err |= close(fd);
			if (err && GDKunlink(farmid, BATDIR, nme, ext) != GDK_SUCCEED) {
				/* do not tolerate corrupt heap images
				 * (BBPrecover on restart will kill
				 * them) */
				GDKfatal("GDKsave: could not open: name=%s, "
					 "ext=%s, mode %d\n", nme,
					 ext ? ext : "", (int) mode);
			}
		} else {
			err = -1;
			GDKerror("GDKsave: failed name=%s, ext=%s, mode %d\n",
				 nme, ext ? ext : "", (int) mode);
		}
	}
	return err ? GDK_FAIL : GDK_SUCCEED;
}

/*
 * Space for the load is directly allocated and the heaps are mapped.
 * Further initialization of the atom heaps require a separate action
 * defined in their implementation.
 *
 * size -- how much to read
 * *maxsize -- (in/out) how much to allocate / how much was allocated
 */
char *
GDKload(int farmid, const char *nme, const char *ext, size_t size, size_t *maxsize, storage_t mode)
{
	char *ret = NULL;

	assert(size <= *maxsize);
	IODEBUG {
		fprintf(stderr, "#GDKload: name=%s, ext=%s, mode %d\n", nme, ext ? ext : "", (int) mode);
	}
	if (mode == STORE_MEM) {
		int fd = GDKfdlocate(farmid, nme, "rb", ext);

		if (fd >= 0) {
			char *dst = ret = GDKmalloc(*maxsize);
			ssize_t n_expected, n = 0;

			if (ret) {
				/* read in chunks, some OSs do not
				 * give you all at once and Windows
				 * only accepts int */
				for (n_expected = (ssize_t) size; n_expected > 0; n_expected -= n) {
					n = read(fd, dst, (unsigned) MIN(1 << 30, n_expected));
					if (n < 0)
						GDKsyserror("GDKload: cannot read: name=%s, ext=%s, " SZFMT " bytes missing.\n", nme, ext ? ext : "", (size_t) n_expected);
#ifndef STATIC_CODE_ANALYSIS
					/* Coverity doesn't seem to
					 * recognize that we're just
					 * printing the value of ptr,
					 * not its contents */
					IODEBUG fprintf(stderr, "#read(dst " PTRFMT ", n_expected " SSZFMT ", fd %d) = " SSZFMT "\n", PTRFMTCAST(void *)dst, n_expected, fd, n);
#endif

					if (n <= 0)
						break;
					dst += n;
				}
				if (n_expected > 0) {
					/* we couldn't read all, error
					 * already generated */
					GDKfree(ret);
					ret = NULL;
				}
#ifndef NDEBUG
				/* just to make valgrind happy, we
				 * initialize the whole thing */
				if (ret && *maxsize > size)
					memset(ret + size, 0, *maxsize - size);
#endif
			}
			close(fd);
		} else {
			GDKerror("GDKload: cannot open: name=%s, ext=%s\n", nme, ext ? ext : "");
		}
	} else {
		char *path;

		/* round up to multiple of GDK_mmap_pagesize with a
		 * minimum of one */
		size = (*maxsize + GDK_mmap_pagesize - 1) & ~(GDK_mmap_pagesize - 1);
		if (size == 0)
			size = GDK_mmap_pagesize;
		path = GDKfilepath(farmid, BATDIR, nme, ext);
		if (path != NULL && GDKextend(path, size) == GDK_SUCCEED) {
			int mod = MMAP_READ | MMAP_WRITE | MMAP_SEQUENTIAL | MMAP_SYNC;

			if (mode == STORE_PRIV)
				mod |= MMAP_COPY;
			ret = GDKmmap(path, mod, size);
			if (ret != NULL) {
				/* success: update allocated size */
				*maxsize = size;
			}
			IODEBUG fprintf(stderr, "#mmap(NULL, 0, maxsize " SZFMT ", mod %d, path %s, 0) = " PTRFMT "\n", size, mod, path, PTRFMTCAST(void *)ret);
		}
		GDKfree(path);
	}
	return ret;
}

/*
 * @+ BAT disk storage
 *
 * Between sessions the BATs comprising the database are saved on
 * disk.  To simplify code, we assume a UNIX directory called its
 * physical @%home@ where they are to be located.  The subdirectories
 * BAT and PRG contain what its name says.
 *
 * A BAT created by @%BATnew@ is considered temporary until one calls
 * the routine @%BATsave@. This routine reserves disk space and checks
 * for name clashes.
 *
 * Saving and restoring BATs is left to the upper layers. The library
 * merely copies the data into place.  Failure to read or write the
 * BAT results in a NULL, otherwise it returns the BAT pointer.
 */
static BATstore *
DESCload(int i)
{
	str s, nme = BBP_physical(i);
	BATstore *bs;
	BAT *b = NULL;
	int ht, tt;

	IODEBUG {
		fprintf(stderr, "#DESCload %s\n", nme ? nme : "<noname>");
	}
	bs = BBP_desc(i);

	if (bs == NULL)
		return 0;
	b = &bs->B;

	ht = b->htype;
	tt = b->ttype;
	if ((ht < 0 && (ht = ATOMindex(s = ATOMunknown_name(ht))) < 0) ||
	    (tt < 0 && (tt = ATOMindex(s = ATOMunknown_name(tt))) < 0)) {
		GDKerror("DESCload: atom '%s' unknown, in BAT '%s'.\n", s, nme);
		return NULL;
	}
	b->htype = ht;
	b->ttype = tt;
	b->H->hash = b->T->hash = NULL;
	/* mil shouldn't mess with just loaded bats */
	if (b->batStamp > 0)
		b->batStamp = -b->batStamp;

	/* reconstruct mode from BBP status (BATmode doesn't flush
	 * descriptor, so loaded mode may be stale) */
	b->batPersistence = (BBP_status(b->batCacheid) & BBPPERSISTENT) ? PERSISTENT : TRANSIENT;
	b->batCopiedtodisk = 1;
	DESCclean(b);
	return bs;
}

void
DESCclean(BAT *b)
{
	b->batDirtyflushed = DELTAdirty(b) ? TRUE : FALSE;
	b->batDirty = 0;
	b->batDirtydesc = 0;
	b->H->heap.dirty = 0;
	b->T->heap.dirty = 0;
	if (b->H->vheap)
		b->H->vheap->dirty = 0;
	if (b->T->vheap)
		b->T->vheap->dirty = 0;
}

/* spawning the background msync should be done carefully 
 * because there is a (small) chance that the BAT has been
 * deleted by the time you issue the msync.
 * This leaves you with possibly deadbeef BAT descriptors.
 */

/* #define DISABLE_MSYNC */
#define MSYNC_BACKGROUND

#ifndef DISABLE_MSYNC
struct msync {
	bat id;
	Heap *h;
};

static void
BATmsyncImplementation(void *arg)
{
	Heap *h = ((struct msync *) arg)->h;
	char *adr;
	size_t len;
	size_t offset;

	adr = h->base;
	offset = ((size_t) adr % MT_pagesize());
	len = MT_pagesize() * (1 + ((h->base + h->free - adr) / MT_pagesize()));
	if (offset)
		adr -= MT_pagesize() - offset;
	if (len)
		(void) MT_msync(adr, len);
	BBPunfix(((struct msync *) arg)->id);
	GDKfree(arg);
}
#endif

void
BATmsync(BAT *b)
{
#ifndef DISABLE_MSYNC
#ifdef MSYNC_BACKGROUND
	MT_Id tid;
#endif
	struct msync *arg;

	assert(b->batPersistence == PERSISTENT);
	if (b->T->heap.storage == STORE_MMAP &&
	    (arg = GDKmalloc(sizeof(*arg))) != NULL) {
		arg->id = b->batCacheid;
		arg->h = &b->T->heap;
		BBPfix(b->batCacheid);
#ifdef MSYNC_BACKGROUND
		if (MT_create_thread(&tid, BATmsyncImplementation, arg, MT_THR_DETACHED) < 0) {
			/* don't bother if we can't create a thread */
			BBPunfix(b->batCacheid);
			GDKfree(arg);
		}
#else
		BATmsyncImplementation(arg);
#endif
	}

	if (b->T->vheap && b->T->vheap->storage == STORE_MMAP &&
	    (arg = GDKmalloc(sizeof(*arg))) != NULL) {
		arg->id = b->batCacheid;
		arg->h = b->T->vheap;
		BBPfix(b->batCacheid);
#ifdef MSYNC_BACKGROUND
		if (MT_create_thread(&tid, BATmsyncImplementation, arg, MT_THR_DETACHED) < 0) {
			/* don't bother if we can't create a thread */
			BBPunfix(b->batCacheid);
			GDKfree(arg);
		}
#else
		BATmsyncImplementation(arg);
#endif
	}
#else
	(void) b;
#endif	/* DISABLE_MSYNC */
}

gdk_return
BATsave(BAT *bd)
{
	gdk_return err = GDK_SUCCEED;
	char *nme;
	BATstore bs;
	BAT *b = bd;

	BATcheck(b, "BATsave", GDK_FAIL);

	/* views cannot be saved, but make an exception for
	 * force-remapped views */
	if (isVIEW(b) &&
	    !(b->H->heap.copied && b->H->heap.storage == STORE_MMAP) &&
	    !(b->T->heap.copied && b->T->heap.storage == STORE_MMAP)) {
		GDKerror("BATsave: %s is a view on %s; cannot be saved\n", BATgetId(b), VIEWhparent(b) ? BBPname(VIEWhparent(b)) : BBPname(VIEWtparent(b)));
		return GDK_FAIL;
	}
	if (!BATdirty(b)) {
		return GDK_SUCCEED;
	}
	if (b->batCacheid < 0) {
		b = BATmirror(b);
	}
	if (!DELTAdirty(b))
		ALIGNcommit(b);
	if (!b->halign)
		b->halign = OIDnew(1);
	if (!b->talign)
		b->talign = OIDnew(1);

	/* copy the descriptor to a local variable in order to let our
	 * messing in the BAT descriptor not affect other threads that
	 * only read it. */
	bs = *BBP_desc(b->batCacheid);
	/* fix up internal pointers */
	b = &bs.BM;		/* first the mirror */
	b->S = &bs.S;
	b->H = &bs.T;
	b->T = &bs.H;
	b = &bs.B;		/* then the unmirrored version */
	b->S = &bs.S;
	b->H = &bs.H;
	b->T = &bs.T;

	if (b->H->vheap) {
		b->H->vheap = (Heap *) GDKmalloc(sizeof(Heap));
		if (b->H->vheap == NULL)
			return GDK_FAIL;
		*b->H->vheap = *bd->H->vheap;
	}
	if (b->T->vheap) {
		b->T->vheap = (Heap *) GDKmalloc(sizeof(Heap));
		if (b->T->vheap == NULL) {
			if (b->H->vheap)
				GDKfree(b->H->vheap);
			return GDK_FAIL;
		}
		*b->T->vheap = *bd->T->vheap;
	}

	/* start saving data */
	nme = BBP_physical(b->batCacheid);
	if (b->batCopiedtodisk == 0 || b->batDirty || b->H->heap.dirty)
		if (err == GDK_SUCCEED && b->htype)
			err = HEAPsave(&b->H->heap, nme, "head");
	if (b->batCopiedtodisk == 0 || b->batDirty || b->T->heap.dirty)
		if (err == GDK_SUCCEED && b->ttype)
			err = HEAPsave(&b->T->heap, nme, "tail");
	if (b->H->vheap && (b->batCopiedtodisk == 0 || b->batDirty || b->H->vheap->dirty))
		if (b->htype && b->hvarsized) {
			if (err == GDK_SUCCEED)
				err = HEAPsave(b->H->vheap, nme, "hheap");
		}
	if (b->T->vheap && (b->batCopiedtodisk == 0 || b->batDirty || b->T->vheap->dirty))
		if (b->ttype && b->tvarsized) {
			if (err == GDK_SUCCEED)
				err = HEAPsave(b->T->vheap, nme, "theap");
		}

	if (b->H->vheap)
		GDKfree(b->H->vheap);
	if (b->T->vheap)
		GDKfree(b->T->vheap);

	if (err == GDK_SUCCEED) {
		bd->batCopiedtodisk = 1;
		DESCclean(bd);
		return GDK_SUCCEED;
	}
	return err;
}


/*
 * TODO: move to gdk_bbp.mx
 */
BAT *
BATload_intern(bat i, int lock)
{
	bat bid = abs(i);
	str nme = BBP_physical(bid);
	BATstore *bs = DESCload(bid);
	BAT *b;

	if (bs == NULL) {
		return NULL;
	}
	b = &bs->B;

	/* LOAD bun heap */
	if (b->htype != TYPE_void) {
		if (HEAPload(&b->H->heap, nme, "head", b->batRestricted == BAT_READ) != GDK_SUCCEED) {
			return NULL;
		}
		assert(b->H->heap.size >> b->H->shift <= BUN_MAX);
		b->batCapacity = (BUN) (b->H->heap.size >> b->H->shift);
	} else {
		b->H->heap.base = NULL;
	}
	if (b->ttype != TYPE_void) {
		if (HEAPload(&b->T->heap, nme, "tail", b->batRestricted == BAT_READ) != GDK_SUCCEED) {
			HEAPfree(&b->H->heap, 0);
			return NULL;
		}
		if (b->htype == TYPE_void) {
			assert(b->T->heap.size >> b->T->shift <= BUN_MAX);
			b->batCapacity = (BUN) (b->T->heap.size >> b->T->shift);
		}
		if (b->batCapacity != (b->T->heap.size >> b->T->shift)) {
			BUN cap = b->batCapacity;
			gdk_return h;
			if (cap < (b->T->heap.size >> b->T->shift)) {
				cap = (BUN) (b->T->heap.size >> b->T->shift);
				HEAPDEBUG fprintf(stderr, "#HEAPextend in BATload_inter %s " SZFMT " " SZFMT "\n", b->H->heap.filename, b->H->heap.size, headsize(b, cap));
				h = HEAPextend(&b->H->heap, headsize(b, cap), b->batRestricted == BAT_READ);
				b->batCapacity = cap;
			} else {
				HEAPDEBUG fprintf(stderr, "#HEAPextend in BATload_intern %s " SZFMT " " SZFMT "\n", b->T->heap.filename, b->T->heap.size, tailsize(b, cap));
				h = HEAPextend(&b->T->heap, tailsize(b, cap), b->batRestricted == BAT_READ);
			}
			if (h != GDK_SUCCEED) {
				HEAPfree(&b->H->heap, 0);
				HEAPfree(&b->T->heap, 0);
				return NULL;
			}
		}
	} else {
		b->T->heap.base = NULL;
	}

	/* LOAD head heap */
	if (ATOMvarsized(b->htype)) {
		if (HEAPload(b->H->vheap, nme, "hheap", b->batRestricted == BAT_READ) != GDK_SUCCEED) {
			HEAPfree(&b->H->heap, 0);
			HEAPfree(&b->T->heap, 0);
			return NULL;
		}
		if (ATOMstorage(b->htype) == TYPE_str) {
			strCleanHash(b->H->vheap, FALSE);	/* ensure consistency */
		}
	}

	/* LOAD tail heap */
	if (ATOMvarsized(b->ttype)) {
		if (HEAPload(b->T->vheap, nme, "theap", b->batRestricted == BAT_READ) != GDK_SUCCEED) {
			if (b->H->vheap)
				HEAPfree(b->H->vheap, 0);
			HEAPfree(&b->H->heap, 0);
			HEAPfree(&b->T->heap, 0);
			return NULL;
		}
		if (ATOMstorage(b->ttype) == TYPE_str) {
			strCleanHash(b->T->vheap, FALSE);	/* ensure consistency */
		}
	}

	/* initialize descriptor */
	b->batDirtydesc = FALSE;
	b->H->heap.parentid = b->T->heap.parentid = 0;

	/* load succeeded; register it in BBP */
	BBPcacheit(bs, lock);

	if (!DELTAdirty(b)) {
		ALIGNcommit(b);
	}

	if ((b->batRestricted == BAT_WRITE && (GDKdebug & CHECKMASK)) ||
	    (GDKdebug & PROPMASK)) {
		++b->batSharecnt;
		--b->batSharecnt;
	}
	return (i < 0) ? BATmirror(b) : b;
}

/*
 * @- BATdelete
 * The new behavior is to let the routine produce warnings but always
 * succeed.  rationale: on a delete, we must get rid of *all* the
 * files. We do not have to care about preserving them or be too much
 * concerned if a file that had to be deleted was not found (end
 * result is still that it does not exist). The past behavior to
 * delete some files and then fail was erroneous. The BAT would
 * continue to exist with an incorrect disk status, causing havoc
 * later on.
 *
 * NT forces us to close all files before deleting them; in case of
 * memory mapped files this means that we have to unload the BATs
 * before deleting. This is enforced now.
 */
void
BATdelete(BAT *b)
{
	bat bid = abs(b->batCacheid);
	str o = BBP_physical(bid);
	BAT *loaded = BBP_cache(bid);

	if (loaded) {
		b = loaded;
		HASHdestroy(b);
		IMPSdestroy(b);
	}
	assert(!b->H->heap.base || !b->T->heap.base || b->H->heap.base != b->T->heap.base);
	if (b->batCopiedtodisk || (b->H->heap.storage != STORE_MEM)) {
		if (b->htype != TYPE_void &&
		    HEAPdelete(&b->H->heap, o, "head") &&
		    b->batCopiedtodisk)
			IODEBUG fprintf(stderr, "#BATdelete(%s): bun heap\n", BATgetId(b));
	} else if (b->H->heap.base) {
		HEAPfree(&b->H->heap, 1);
	}
	if (b->batCopiedtodisk || (b->T->heap.storage != STORE_MEM)) {
		if (b->ttype != TYPE_void &&
		    HEAPdelete(&b->T->heap, o, "tail") &&
		    b->batCopiedtodisk)
			IODEBUG fprintf(stderr, "#BATdelete(%s): bun heap\n", BATgetId(b));
	} else if (b->T->heap.base) {
		HEAPfree(&b->T->heap, 1);
	}
	if (b->H->vheap) {
		assert(b->H->vheap->parentid == bid);
		if (b->batCopiedtodisk || (b->H->vheap->storage != STORE_MEM)) {
			if (HEAPdelete(b->H->vheap, o, "hheap") && b->batCopiedtodisk)
				IODEBUG fprintf(stderr, "#BATdelete(%s): head heap\n", BATgetId(b));
		} else {
			HEAPfree(b->H->vheap, 1);
		}
	}
	if (b->T->vheap) {
		assert(b->T->vheap->parentid == bid);
		if (b->batCopiedtodisk || (b->T->vheap->storage != STORE_MEM)) {
			if (HEAPdelete(b->T->vheap, o, "theap") && b->batCopiedtodisk)
				IODEBUG fprintf(stderr, "#BATdelete(%s): tail heap\n", BATgetId(b));
		} else {
			HEAPfree(b->T->vheap, 1);
		}
	}
	b->batCopiedtodisk = FALSE;
}

/*
 * BAT specific printing
 */

gdk_return
BATprintcolumns(stream *s, int argc, BAT *argv[])
{
	int i;
	BUN n, cnt;
	struct colinfo {
		int (*s) (str *, int *, const void *);
		BATiter i;
	} *colinfo;
	char *buf;
	int buflen = 0;
	int len;

	/* error checking */
	for (i = 0; i < argc; i++) {
		if (argv[i] == NULL) {
			GDKerror("Columns missing\n");
			return GDK_FAIL;
		}
		if (BATcount(argv[0]) != BATcount(argv[i])) {
			GDKerror("Columns must be the same size\n");
			return GDK_FAIL;
		}
	}

	if ((colinfo = GDKmalloc(argc * sizeof(*colinfo))) == NULL) {
		GDKerror("Cannot allocate memory\n");
		return GDK_FAIL;
	}

	for (i = 0; i < argc; i++) {
		colinfo[i].i = bat_iterator(argv[i]);
		colinfo[i].s = BATatoms[argv[i]->ttype].atomToStr;
	}

	mnstr_write(s, "#--------------------------#\n", 1, 29);
	mnstr_write(s, "# ", 1, 2);
	for (i = 0; i < argc; i++) {
		if (i > 0)
			mnstr_write(s, "\t", 1, 1);
		buf = argv[i]->tident;
		mnstr_write(s, buf, 1, strlen(buf));
	}
	mnstr_write(s, "  # name\n", 1, 9);
	mnstr_write(s, "# ", 1, 2);
	for (i = 0; i < argc; i++) {
		if (i > 0)
			mnstr_write(s, "\t", 1, 1);
		buf = ATOMname(argv[i]->ttype);
		mnstr_write(s, buf, 1, strlen(buf));
	}
	mnstr_write(s, "  # type\n", 1, 9);
	mnstr_write(s, "#--------------------------#\n", 1, 29);
	buf = NULL;

	for (n = 0, cnt = BATcount(argv[0]); n < cnt; n++) {
		mnstr_write(s, "[ ", 1, 2);
		for (i = 0; i < argc; i++) {
			len = colinfo[i].s(&buf, &buflen, BUNtail(colinfo[i].i, BUNfirst(argv[i]) + n));
			if (i > 0)
				mnstr_write(s, ",\t", 1, 2);
			mnstr_write(s, buf, 1, len);
		}
		mnstr_write(s, "  ]\n", 1, 4);
	}

	GDKfree(buf);
	GDKfree(colinfo);

	return GDK_SUCCEED;
}

gdk_return
BATprintf(stream *s, BAT *b)
{
	BAT *argv[2];
	gdk_return ret = GDK_FAIL;

	argv[0] = BATdense(b->hseqbase, b->hseqbase, BATcount(b));
	argv[1] = b;
	if (argv[0] && argv[1]) {
		BATroles(argv[0], NULL, b->hident);
		ret = BATprintcolumns(s, 2, argv);
	}
	if (argv[0])
		BBPunfix(argv[0]->batCacheid);
	return ret;
}

gdk_return
BATprint(BAT *b)
{
	return BATprintf(GDKstdout, b);
}

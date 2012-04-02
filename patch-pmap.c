--- pmap/pmap.c.orig	2006-06-22 06:55:17.000000000 -0500
+++ pmap/pmap.c	2012-04-01 19:14:51.000000000 -0500
@@ -52,7 +52,7 @@
 #include "kernel.h"
 
 /* total memory usage for process */
-u_long p_msize = 0, p_rsize = 0, p_ssize = 0, p_psize = 0;
+u_long p_msize, p_rsize, p_ssize, p_psize, p_prsize;
 
 /* process memory map size */
 u_long	map_size;
@@ -83,15 +83,17 @@
 	char	*prog_wild = NULL, *prog_name;
 	regex_t preg;
 	struct pargs *pa = NULL;
-    struct kinfo_proc *kp;
-    int	pmap_helper_syscall;
-	
-    if ((modid = modfind("pmap_helper")) == -1) { 
+	struct kinfo_proc *kp;
+	int	pmap_helper_syscall;
+
+	p_msize = 0, p_rsize = 0, p_ssize = 0, p_psize = 0, p_prsize = 0;
+
+	if ((modid = modfind("sys/pmap_helper")) == -1) { 
 		/* module not found, try to load */
 		modid = kldload("pmap_helper.ko");
 		if (modid == -1) 
 			err(1, "unable to load pmap_helper module");
-		modid = modfind("pmap_helper");
+		modid = modfind("sys/pmap_helper");
 		if (modid == -1)
 			err(1, "pmap_helper module loaded but not found");
 	}
@@ -102,17 +104,17 @@
 
 	while ((c = getopt(argc, argv, "n:")) != EOF) {
 		switch (c) {
-		case 'n':
-			prog_wild = optarg;
-			break;
-		default:
-			usage();
-			break;
+			case 'n':
+				prog_wild = optarg;
+				break;
+			default:
+				usage();
+				break;
 		}
 	}
 	if (prog_wild != NULL) {
 		if (0 != regcomp(&preg, prog_wild,
-			REG_EXTENDED | REG_ICASE | REG_NOSUB)) {
+					REG_EXTENDED | REG_ICASE | REG_NOSUB)) {
 			errx(1, "regcomp() failed\n");
 		}
 	} else if ((argc != 2) || ((pid = atoi(argv[1])) == 0)) {
@@ -173,27 +175,28 @@
 		 * process. If string contains 'ELF64' we assume
 		 * LP64 mode, if not - ILP32.
 		 */
-		 if (strstr(kp[prc].ki_emul, "ELF64") != NULL) {
+		if (strstr(kp[prc].ki_emul, "ELF64") != NULL) {
 			/* process is 64bit ELF executable */
 			a_wdth = 16;
 			s_wdth = 10;
 			bar = bar16;
-		 } else {
+		} else {
 			a_wdth = 8;
 			s_wdth = 7;
 			bar = bar8;
-		 }
+		}
 
 		printf("%-*s %*s %*s %*s %*s %-5s %s\n",
-		    a_wdth, "Address", s_wdth, "Kbytes",
-	            s_wdth, "RSS", s_wdth, "Shared",
-		    s_wdth, "Priv", 
-		    "Mode", "Mapped File");
-		p_msize = p_rsize = p_ssize = p_psize = 0;
-	
+				a_wdth, "Address", s_wdth, "Kbytes",
+				s_wdth, "RSS", s_wdth, "Shared",
+				s_wdth, "Priv", 
+				"Mode", "Mapped File");
+		p_msize = p_rsize = p_ssize = p_psize = p_prsize = 0;
+
 		for (i=0; i < pmh.nmaps; printf("\n"), i++) {
 			/*
 			 * Skips maps with no lower objects
+			 * If the printout shows a blank line, that's why
 			 */
 			if (!maps[i].lobj_present)
 				continue;
@@ -203,12 +206,13 @@
 			process_map(&maps[i]);
 		}
 		printf("%s%s %s %s %s %s\n",
-		    a_wdth == 8 ? "-" : "------", bar, bar, bar, bar, bar);
+				a_wdth == 8 ? "-" : "------", bar, bar, bar, bar, bar);
 		printf("%-*s %*lu %*lu %*lu %*lu\n\n",
-		    a_wdth, "Total Kb", s_wdth, p_msize, s_wdth, p_rsize,
-		    s_wdth, p_ssize, s_wdth, p_psize);
+				a_wdth, "Total Kb", s_wdth, p_msize, s_wdth, p_rsize,
+				s_wdth, p_ssize, s_wdth, p_psize);
+		printf("Private Resident Pages: %lu\n\n", p_prsize);
 	}
-		
+
 	k_close();
 	if (prog_wild != NULL)
 		regfree(&preg);
@@ -231,59 +235,69 @@
 	 * output: resident, shared, priv, proto, mapped file 
 	 */
 	switch (lobj->type) {
-	case OBJT_DEFAULT: /* XXX treat all memory as private. Is it 
-			      always correct? 
-			   */
-		printf("%*lu %*s %*lu ", s_wdth, m.resident_pages * NKPG, 
-		    s_wdth, "-", s_wdth, map_size); 
-		p_rsize += m.resident_pages * NKPG;
-		p_psize += map_size;
-		print_proto(MAP_VME(m).protection);       
-		printf("  [ anon ]");
-		break;
-
-	case OBJT_VNODE:
-	        pkbytes = (obj->shadow_count == 1) ? 
-		  (u_long)(obj->resident_page_count * NKPG) :
-	           m.resident_pages * NKPG ;
-		printf("%*lu ", s_wdth, pkbytes); 
-		p_rsize +=  pkbytes;
-		if (obj->shadow_count == 1) {
-		  printf("%*s %*lu ", s_wdth, "-", s_wdth, pkbytes);
-			p_psize += pkbytes;
-		} else if (obj->ref_count == 1) { /* XXX does it alawys mean private mapping (?) */
-			printf("%*s %*lu ", s_wdth, "-", s_wdth, map_size);
+		case OBJT_DEFAULT: /* XXX treat all memory as private. Is it 
+				      always correct? 
+				    */
+			printf("%*lu %*s %*lu ", s_wdth, m.resident_pages * NKPG, 
+					s_wdth, "-", s_wdth, map_size); 
+			p_rsize += m.resident_pages * NKPG;
+			p_prsize += m.resident_pages;
 			p_psize += map_size;
-		} else {
-			printf("%*lu %*s ", s_wdth, map_size, s_wdth, "-");
-			p_ssize += map_size;
-		}
-		print_proto(MAP_VME(m).protection);       
-        printf("%s", m.fname);
-		break;
-
-	case OBJT_SWAP:
-		printf("%*s %*s %*s ", s_wdth, "-", s_wdth, "-", s_wdth, "-");
-		print_proto(MAP_VME(m).protection);       
-		printf("[swap pager]"); 
-		break;
-
-	case OBJT_DEVICE:
-		printf("%*lu %*s %*lu ", s_wdth, m.resident_pages * NKPG,
-                       s_wdth, "-", s_wdth, map_size);
-		print_proto(MAP_VME(m).protection);
-		printf("/dev/%s", m.fname);
-		break;
-
-	case OBJT_DEAD:
-                printf("%*s %*s %*s ", s_wdth, "-", s_wdth, "-", s_wdth, "-");
-		print_proto(MAP_VME(m).protection);       
-		printf("[dead object]");
-		break;
-
-	default:
-		printf("[unknown (%i) object type]", lobj->type);
-		break;
+			print_proto(MAP_VME(m).protection);       
+			printf("  [ anon ]");
+			printf(" \t[%lu] ", p_prsize);
+			break;
+
+		case OBJT_VNODE:
+			pkbytes = (obj->shadow_count == 1) ? 
+				(u_long)(obj->resident_page_count * NKPG) :
+				m.resident_pages * NKPG ;
+			printf("%*lu ", s_wdth, pkbytes); 
+			p_rsize +=  pkbytes;
+			if (obj->shadow_count == 1) {
+				printf("%*s %*lu ", s_wdth, "-", s_wdth, pkbytes);
+				p_psize += pkbytes;
+				p_prsize += (u_long)(obj->resident_page_count);
+			} else if (obj->ref_count == 1) { /* XXX does it alawys mean private mapping (?) */
+				printf("%*s %*lu ", s_wdth, "-", s_wdth, map_size);
+				p_psize += map_size;
+				p_prsize += m.resident_pages; /* pkbytes without NKPG */
+			} else {
+				printf("%*lu %*s ", s_wdth, map_size, s_wdth, "-");
+				p_ssize += map_size;
+			}
+			print_proto(MAP_VME(m).protection);       
+			printf("%s", m.fname);
+			printf(" \t[%lu] ", p_prsize);
+			break;
+
+		case OBJT_SWAP:
+			printf("%*s %*s %*s ", s_wdth, "-", s_wdth, "-", s_wdth, "-");
+			print_proto(MAP_VME(m).protection);       
+			printf("[swap pager]"); 
+			printf(" \t[%lu] ", p_prsize);
+			break;
+
+		case OBJT_DEVICE:
+			p_prsize += m.resident_pages;
+			printf("%*lu %*s %*lu ", s_wdth, m.resident_pages * NKPG,
+					s_wdth, "-", s_wdth, map_size);
+			print_proto(MAP_VME(m).protection);
+			printf("/dev/%s", m.fname);
+			printf(" \t[%lu] ", p_prsize);
+			break;
+
+		case OBJT_DEAD:
+			printf("%*s %*s %*s ", s_wdth, "-", s_wdth, "-", s_wdth, "-");
+			print_proto(MAP_VME(m).protection);       
+			printf("[dead object]");
+			printf(" \t[%lu] ", p_prsize);
+			break;
+
+		default:
+			printf("[unknown (%i) object type]", lobj->type);
+			printf(" \t[%lu] ", p_prsize);
+			break;
 	}
 }
 

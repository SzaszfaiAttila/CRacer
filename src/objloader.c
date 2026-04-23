#include "objloader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ── Scratch limits ──────────────────────────────────────────────────────── */
#define OBJ_MAX_POS    65536
#define OBJ_MAX_NRM    65536
#define OBJ_MAX_TRI   131072
#define MTL_MAX_MATS      64
#define MTL_NAME_LEN      64

/* ── MTL material record ─────────────────────────────────────────────────── */
typedef struct {
    char  name[MTL_NAME_LEN];
    float kd[3];   /* diffuse  */
    float ks[3];   /* specular */
    float ns;      /* shininess exponent */
} MtlMat;

/* ── Per-triangle record ─────────────────────────────────────────────────── */
typedef struct { int pi[3]; int ni[3]; int mat; } OTri;

/* ── Static scratch (loader is single-threaded, startup only) ────────────── */
static float  g_px[OBJ_MAX_POS], g_py[OBJ_MAX_POS], g_pz[OBJ_MAX_POS];
static float  g_nx[OBJ_MAX_NRM], g_ny[OBJ_MAX_NRM], g_nz[OBJ_MAX_NRM];
static OTri   g_tris[OBJ_MAX_TRI];
static int    g_np, g_nn, g_nt;
static MtlMat g_mats[MTL_MAX_MATS];
static int    g_nm;

/* ── String helpers ──────────────────────────────────────────────────────── */
static void trim_nl(char *s) {
    char *p = s + strlen(s);
    while (p > s && (p[-1] == '\n' || p[-1] == '\r' || p[-1] == ' ')) *--p = '\0';
}

/* Build the path to the MTL file next to the OBJ file */
static void make_mtl_path(const char *obj_path, const char *mtl_name,
                           char *out, size_t outsz) {
    /* Copy directory part of obj_path */
    const char *last_slash = NULL;
    for (const char *p = obj_path; *p; p++)
        if (*p == '/' || *p == '\\') last_slash = p;
    size_t dirlen = last_slash ? (size_t)(last_slash + 1 - obj_path) : 0;
    if (dirlen >= outsz - 1) dirlen = outsz - 2;
    strncpy(out, obj_path, dirlen);
    out[dirlen] = '\0';
    strncat(out, mtl_name, outsz - dirlen - 1);
}

/* ── MTL parser ──────────────────────────────────────────────────────────── */
static void load_mtl(const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) { fprintf(stderr, "OBJ: cannot open MTL '%s'\n", path); return; }

    int cur = -1;
    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        trim_nl(line);
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;

        if (strncmp(p, "newmtl ", 7) == 0) {
            if (g_nm < MTL_MAX_MATS) {
                cur = g_nm++;
                MtlMat *m = &g_mats[cur];
                strncpy(m->name, p + 7, MTL_NAME_LEN - 1);
                m->name[MTL_NAME_LEN - 1] = '\0';
                /* defaults */
                m->kd[0] = m->kd[1] = m->kd[2] = 1.0f;
                m->ks[0] = m->ks[1] = m->ks[2] = 0.5f;
                m->ns    = 32.0f;
            }
        } else if (cur >= 0) {
            if (strncmp(p, "Kd ", 3) == 0) {
                sscanf(p + 3, "%f %f %f",
                       &g_mats[cur].kd[0],
                       &g_mats[cur].kd[1],
                       &g_mats[cur].kd[2]);
            } else if (strncmp(p, "Ks ", 3) == 0) {
                sscanf(p + 3, "%f %f %f",
                       &g_mats[cur].ks[0],
                       &g_mats[cur].ks[1],
                       &g_mats[cur].ks[2]);
            } else if (strncmp(p, "Ns ", 3) == 0) {
                sscanf(p + 3, "%f", &g_mats[cur].ns);
            }
        }
    }
    fclose(fp);
    printf("OBJ: loaded MTL '%s' (%d materials)\n", path, g_nm);
}

/* ── Find material index by name ─────────────────────────────────────────── */
static int find_mat(const char *name) {
    for (int i = 0; i < g_nm; i++)
        if (strcmp(g_mats[i].name, name) == 0) return i;
    return (g_nm > 0) ? 0 : -1;
}

/* ── Face-vertex token parser ────────────────────────────────────────────── */
static void parse_fv(const char *tok, int *out_pi, int *out_ni) {
    *out_pi = 0; *out_ni = 0;
    *out_pi = atoi(tok);
    const char *sl = strchr(tok, '/');
    if (!sl) return;
    const char *sl2 = strchr(sl + 1, '/');
    if (sl2 && sl2[1] != '\0' && sl2[1] != ' ' && sl2[1] != '\n')
        *out_ni = atoi(sl2 + 1);
}

/* ── Public API ──────────────────────────────────────────────────────────── */
ObjModel obj_load(const char *path) {
    ObjModel out;
    memset(&out, 0, sizeof(out));
    /* Default surface: white, mild specular */
    out.kd_r = out.kd_g = out.kd_b = 1.0f;
    out.ks_r = out.ks_g = out.ks_b = 0.5f;
    out.ns   = 32.0f;

    FILE *fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "OBJ: cannot open '%s'\n", path);
        return out;
    }

    g_np = g_nn = g_nt = g_nm = 0;

    char line[1024];
    int  cur_mat = -1;

    while (fgets(line, sizeof(line), fp)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;

        if (p[0] == 'v' && p[1] == 'n' && p[2] == ' ') {
            if (g_nn < OBJ_MAX_NRM) {
                sscanf(p + 3, "%f %f %f",
                       &g_nx[g_nn], &g_ny[g_nn], &g_nz[g_nn]);
                g_nn++;
            }
        } else if (p[0] == 'v' && p[1] == ' ') {
            if (g_np < OBJ_MAX_POS) {
                sscanf(p + 2, "%f %f %f",
                       &g_px[g_np], &g_py[g_np], &g_pz[g_np]);
                g_np++;
            }
        } else if (strncmp(p, "mtllib ", 7) == 0) {
            char mtlname[512];
            sscanf(p + 7, "%511s", mtlname);
            trim_nl(mtlname);
            char mtlpath[1024];
            make_mtl_path(path, mtlname, mtlpath, sizeof(mtlpath));
            load_mtl(mtlpath);

        } else if (strncmp(p, "usemtl ", 7) == 0) {
            char name[MTL_NAME_LEN];
            sscanf(p + 7, "%63s", name);
            trim_nl(name);
            cur_mat = find_mat(name);

        } else if (p[0] == 'f' && p[1] == ' ') {
            int fpi[8], fni[8], fc = 0;
            char *q = p + 2;
            while (*q && *q != '\n' && *q != '\r' && fc < 8) {
                while (*q == ' ' || *q == '\t') q++;
                if (!*q || *q == '\n' || *q == '\r') break;
                char tok[64]; int ti = 0;
                while (*q && *q != ' ' && *q != '\t' && *q != '\n' && *q != '\r')
                    tok[ti++] = *q++;
                tok[ti] = '\0';
                if (ti > 0) { parse_fv(tok, &fpi[fc], &fni[fc]); fc++; }
            }
            if (fc < 3) continue;

            /* Resolve OBJ 1-based / negative indices to 0-based */
            for (int i = 0; i < fc; i++) {
                fpi[i] = (fpi[i] > 0) ? (fpi[i] - 1) : (g_np + fpi[i]);
                fni[i] = (fni[i] > 0) ? (fni[i] - 1) : (g_nn + fni[i]);
                if (fpi[i] < 0 || fpi[i] >= g_np) fpi[i] = 0;
                if (fni[i] < 0 || fni[i] >= g_nn) fni[i] = 0;
            }

            /* Fan-triangulate */
            for (int i = 1; i < fc - 1; i++) {
                if (g_nt >= OBJ_MAX_TRI) break;
                OTri *t  = &g_tris[g_nt++];
                t->pi[0] = fpi[0];   t->ni[0] = fni[0];
                t->pi[1] = fpi[i];   t->ni[1] = fni[i];
                t->pi[2] = fpi[i+1]; t->ni[2] = fni[i+1];
                t->mat   = cur_mat;
            }
        }
    }
    fclose(fp);

    if (g_nt == 0) {
        fprintf(stderr, "OBJ: no geometry in '%s'\n", path);
        return out;
    }

    /* ── Build interleaved vertex buffer: pos(xyz) + normal(xyz) + colour(rgb)
       The extra rgb per-vertex lets the shader use it as objectColor when
       the material attribute is present.  We bake MTL Kd into the normals
       slot so the existing Mesh / shader pipeline stays unchanged — the GPU
       mesh stores (px,py,pz, nx,ny,nz) exactly as before; colour comes from
       the ObjModel.kd fields and is set via draw_obj / draw_emissive in main.
       ─────────────────────────────────────────────────────────────────────── */
    int float_count = g_nt * 3 * 6;
    float *buf = (float *)malloc((size_t)float_count * sizeof(float));
    if (!buf) { fprintf(stderr, "OBJ: malloc failed\n"); return out; }

    int off = 0;
    for (int i = 0; i < g_nt; i++) {
        for (int k = 0; k < 3; k++) {
            int pi = g_tris[i].pi[k];
            int ni = g_tris[i].ni[k];
            buf[off++] = g_px[pi]; buf[off++] = g_py[pi]; buf[off++] = g_pz[pi];
            buf[off++] = g_nx[ni]; buf[off++] = g_ny[ni]; buf[off++] = g_nz[ni];
        }
    }

    out.mesh = mesh_create(buf, off);
    free(buf);

    /* ── Aggregate material: average Kd/Ks/Ns across all materials,
       weighted by triangle count (gives reasonable result for multi-mat bikes) */
    if (g_nm > 0) {
        /* First count tris per material */
        int counts[MTL_MAX_MATS] = {0};
        for (int i = 0; i < g_nt; i++)
            if (g_tris[i].mat >= 0 && g_tris[i].mat < g_nm)
                counts[g_tris[i].mat]++;

        float total = 0.0f;
        float kr=0,kg=0,kb=0, sr=0,sg=0,sb=0, ns=0;
        for (int m = 0; m < g_nm; m++) {
            float w = (float)counts[m];
            kr += g_mats[m].kd[0]*w; kg += g_mats[m].kd[1]*w; kb += g_mats[m].kd[2]*w;
            sr += g_mats[m].ks[0]*w; sg += g_mats[m].ks[1]*w; sb += g_mats[m].ks[2]*w;
            ns += g_mats[m].ns  *w;
            total += w;
        }
        if (total > 0.5f) {
            out.kd_r = kr/total; out.kd_g = kg/total; out.kd_b = kb/total;
            out.ks_r = sr/total; out.ks_g = sg/total; out.ks_b = sb/total;
            out.ns   = ns/total;
        }
    }

    printf("OBJ: '%s' → %d tris  Kd=(%.2f,%.2f,%.2f)  Ns=%.0f\n",
           path, g_nt, out.kd_r, out.kd_g, out.kd_b, out.ns);
    return out;
}

void obj_free(ObjModel *m) {
    if (m && m->mesh.vert_count > 0)
        mesh_free(&m->mesh);
}

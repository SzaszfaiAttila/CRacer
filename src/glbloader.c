/*  glbloader.c — binary glTF (.glb) loader with per-primitive materials.
 *
 *  Dependencies:
 *    cgltf.h   — single-header glTF parser, MIT licensed
 *                https://github.com/jkuhlmann/cgltf
 *    shader.h  — shader_set_vec3 / shader_set_float helpers
 *
 *  Drop cgltf.h next to this file, add glbloader.c to your build.          */

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

#include "glbloader.h"
#include "shader.h"
#include "mesh.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ── Per-primitive scratch buffer ────────────────────────────────────────── */
/* Supports up to ~87k triangles per primitive. Typical bike part << 10k.   */
#define PRIM_MAX_FLOATS  (87381 * 3 * 6)
static float g_pbuf[PRIM_MAX_FLOATS];

/* ── Helpers ─────────────────────────────────────────────────────────────── */
static void read_vec3(cgltf_accessor *acc, cgltf_size i,
                      float *x, float *y, float *z) {
    if (!acc || i >= acc->count) { *x=0; *y=1; *z=0; return; }
    cgltf_float v[3] = {0,1,0};
    cgltf_accessor_read_float(acc, i, v, 3);
    *x=v[0]; *y=v[1]; *z=v[2];
}

/* Build vertex buffer for one primitive; returns float count written (0=fail) */
static int build_prim_vbuf(cgltf_primitive *prim) {
    cgltf_accessor *pos_acc  = NULL;
    cgltf_accessor *norm_acc = NULL;

    for (cgltf_size ai = 0; ai < prim->attributes_count; ai++) {
        cgltf_attribute *attr = &prim->attributes[ai];
        if      (attr->type == cgltf_attribute_type_position) pos_acc  = attr->data;
        else if (attr->type == cgltf_attribute_type_normal)   norm_acc = attr->data;
    }
    if (!pos_acc) return 0;

    int off = 0;

    if (prim->indices) {
        cgltf_size tri_count = prim->indices->count / 3;
        for (cgltf_size t = 0; t < tri_count; t++) {
            for (int k = 0; k < 3; k++) {
                cgltf_size vi = cgltf_accessor_read_index(prim->indices, t*3+k);
                float px,py,pz, nx,ny,nz;
                read_vec3(pos_acc,  vi, &px, &py, &pz);
                read_vec3(norm_acc, vi, &nx, &ny, &nz);
                if (off+6 > PRIM_MAX_FLOATS) goto done;
                g_pbuf[off++]=px; g_pbuf[off++]=py; g_pbuf[off++]=pz;
                g_pbuf[off++]=nx; g_pbuf[off++]=ny; g_pbuf[off++]=nz;
            }
        }
    } else {
        for (cgltf_size vi = 0; vi < pos_acc->count; vi++) {
            float px,py,pz, nx,ny,nz;
            read_vec3(pos_acc,  vi, &px, &py, &pz);
            read_vec3(norm_acc, vi, &nx, &ny, &nz);
            if (off+6 > PRIM_MAX_FLOATS) goto done;
            g_pbuf[off++]=px; g_pbuf[off++]=py; g_pbuf[off++]=pz;
            g_pbuf[off++]=nx; g_pbuf[off++]=ny; g_pbuf[off++]=nz;
        }
    }
    done:
    return off;
}

/* Parse material from a cgltf_primitive into a GlbPrim (material fields only) */
static void parse_material(cgltf_primitive *prim, GlbPrim *out) {
    out->kd[0] = out->kd[1] = out->kd[2] = 1.0f;
    out->emissive[0] = out->emissive[1] = out->emissive[2] = 0.0f;
    out->roughness = 0.5f;
    out->metallic  = 0.0f;
    out->alpha     = 1.0f;

    if (!prim->material) return;
    cgltf_material *mat = prim->material;

    if (mat->has_pbr_metallic_roughness) {
        cgltf_pbr_metallic_roughness *pbr = &mat->pbr_metallic_roughness;
        out->kd[0]    = pbr->base_color_factor[0];
        out->kd[1]    = pbr->base_color_factor[1];
        out->kd[2]    = pbr->base_color_factor[2];
        out->alpha    = pbr->base_color_factor[3];
        out->roughness = pbr->roughness_factor;
        out->metallic  = pbr->metallic_factor;
    } else if (mat->has_pbr_specular_glossiness) {
        cgltf_pbr_specular_glossiness *sg = &mat->pbr_specular_glossiness;
        out->kd[0]    = sg->diffuse_factor[0];
        out->kd[1]    = sg->diffuse_factor[1];
        out->kd[2]    = sg->diffuse_factor[2];
        out->alpha    = sg->diffuse_factor[3];
        out->roughness = 1.0f - sg->glossiness_factor;
    }

    /* Emissive factor — present in both PBR workflows */
    out->emissive[0] = mat->emissive_factor[0];
    out->emissive[1] = mat->emissive_factor[1];
    out->emissive[2] = mat->emissive_factor[2];
}

/* ── Public API ──────────────────────────────────────────────────────────── */
ObjModel obj_load(const char *path) {
    ObjModel out;
    memset(&out, 0, sizeof(out));
    out.kd_r = out.kd_g = out.kd_b = 1.0f;
    out.ks_r = out.ks_g = out.ks_b = 0.5f;
    out.ns   = 32.0f;

    cgltf_options options; memset(&options, 0, sizeof(options));
    cgltf_data   *data   = NULL;
    cgltf_result  result = cgltf_parse_file(&options, path, &data);
    if (result != cgltf_result_success) {
        fprintf(stderr, "GLB: failed to parse '%s' (error %d)\n", path, (int)result);
        return out;
    }
    if (cgltf_load_buffers(&options, data, path) != cgltf_result_success) {
        fprintf(stderr, "GLB: failed to load buffers '%s'\n", path);
        cgltf_free(data); return out;
    }

    /* ── Process each mesh primitive as a separate sub-mesh ──────────────── */
    float kd_sum[3]  = {0,0,0};
    float rough_sum  = 0.0f;
    float total_tris = 0.0f;

    for (cgltf_size mi = 0; mi < data->meshes_count && out.prim_count < GLB_MAX_PRIMS; mi++) {
        cgltf_mesh *mesh = &data->meshes[mi];
        for (cgltf_size pi = 0; pi < mesh->primitives_count && out.prim_count < GLB_MAX_PRIMS; pi++) {
            cgltf_primitive *prim = &mesh->primitives[pi];
            if (prim->type != cgltf_primitive_type_triangles) continue;

            int fc = build_prim_vbuf(prim);
            if (fc == 0) continue;

            GlbPrim *gp = &out.prims[out.prim_count++];
            gp->mesh = mesh_create(g_pbuf, fc);
            parse_material(prim, gp);

            float w = (float)(fc / 18);   /* triangle count */
            kd_sum[0] += gp->kd[0] * w;
            kd_sum[1] += gp->kd[1] * w;
            kd_sum[2] += gp->kd[2] * w;
            rough_sum  += gp->roughness * w;
            total_tris += w;
        }
    }
    cgltf_free(data);

    if (out.prim_count == 0) {
        fprintf(stderr, "GLB: no geometry in '%s'\n", path);
        return out;
    }

    /* ── Aggregate material for reflection pass ───────────────────────────── */
    if (total_tris > 0.5f) {
        out.kd_r = kd_sum[0] / total_tris;
        out.kd_g = kd_sum[1] / total_tris;
        out.kd_b = kd_sum[2] / total_tris;
        float rough = fmaxf(0.0f, fminf(1.0f, rough_sum / total_tris));
        float spec  = 1.0f - rough;
        out.ks_r = out.ks_g = out.ks_b = spec;
        out.ns   = 4.0f + spec * spec * 252.0f;
    }

    printf("GLB: '%s'  %d prims  avg Kd=(%.2f,%.2f,%.2f)  Ns=%.0f\n",
           path, out.prim_count, out.kd_r, out.kd_g, out.kd_b, out.ns);
    return out;
}

/* ── Draw function ───────────────────────────────────────────────────────── */
void glb_draw_model(const ObjModel *m, GLuint sh, float brightness) {
    if (m->prim_count == 0) return;

    /* Disable back-face culling — fixes see-through on models whose normals
       are exported pointing inward (common Blender GLB export behaviour).
       uTwoSided=1 in the shader handles lighting on both faces correctly.   */
    glDisable(GL_CULL_FACE);

    for (int i = 0; i < m->prim_count; i++) {
        const GlbPrim *gp = &m->prims[i];
        if (gp->mesh.vert_count == 0) continue;

        /* ── Base colour ─────────────────────────────────────────────────── */
        shader_set_vec3(sh, "objectColor", gp->kd[0], gp->kd[1], gp->kd[2]);

        /* ── Emissive ────────────────────────────────────────────────────── */
        /* Use the GLTF emissive factor directly; add a small constant so the
           bike isn't pitch-black in the dark Tron scene.                    */
        float small_rim = 0.05f * brightness;
        shader_set_vec3(sh, "emissive",
                        gp->emissive[0] * brightness + gp->kd[0] * small_rim,
                        gp->emissive[1] * brightness + gp->kd[1] * small_rim,
                        gp->emissive[2] * brightness + gp->kd[2] * small_rim);

        /* ── Ambient ─────────────────────────────────────────────────────── */
        shader_set_float(sh, "ambientStr", 0.35f * brightness);

        /* ── Specular — derived from PBR roughness/metallic ─────────────── */
        float rough  = fmaxf(0.01f, gp->roughness);
        float spec   = (1.0f - rough) * (1.0f + gp->metallic * 0.5f);
        float shine  = 4.0f + (1.0f - rough) * (1.0f - rough) * 252.0f;
        shader_set_float(sh, "uSpecularStr", fminf(spec, 1.0f));
        shader_set_float(sh, "uShininess",   shine);

        mesh_draw((Mesh *)&gp->mesh);
    }

    /* Scene has GL_CULL_FACE globally disabled; do not re-enable here. */

    /* Reset shader state so the next draw call starts clean */
    shader_set_float(sh, "uSpecularStr", 0.0f);
    shader_set_float(sh, "uShininess",   32.0f);
    shader_set_float(sh, "ambientStr",   0.12f * brightness);
}

void obj_free(ObjModel *m) {
    if (!m) return;
    for (int i = 0; i < m->prim_count; i++)
        if (m->prims[i].mesh.vert_count > 0)
            mesh_free(&m->prims[i].mesh);
    memset(m, 0, sizeof(*m));
}

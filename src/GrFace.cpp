#include "GrFace.h"
#include "VMScratch.h"
#include <string.h>
#include "Segment.h"
#include "XmlTraceLog.h"

using namespace org::sil::graphite::v2;

GrFace::~GrFace()
{
    delete m_pGlyphFaceCache;
    delete[] m_silfs;
    m_pGlyphFaceCache = NULL;
    m_silfs = NULL;
}


bool GrFace::setGlyphCacheStrategy(EGlyphCacheStrategy requestedStrategy) const      //glyphs already loaded are unloaded
{
    GlyphFaceCache* pNewCache = GlyphFaceCache::makeCache(*m_pGlyphFaceCache, requestedStrategy);
    if (!pNewCache)
        return false;
    
    delete m_pGlyphFaceCache;
    m_pGlyphFaceCache = pNewCache;
    return true;
}


bool GrFace::readGlyphs(EGlyphCacheStrategy requestedStrategy)
{
    GlyphFaceCacheHeader hdr;
    if (!hdr.initialize(m_appFaceHandle, m_getTable)) return false;

    m_pGlyphFaceCache = GlyphFaceCache::makeCache(hdr, requestedStrategy);

    if (!m_pGlyphFaceCache) return false;
    m_upem = TtfUtil::DesignUnits(m_pGlyphFaceCache->m_pHead);
    // m_glyphidx = new unsigned short[m_numGlyphs];        // only need this if doing occasional glyph reads
    
    return true;
}

bool GrFace::readGraphite()
{
    char *pSilf;
    size_t lSilf;
    if ((pSilf = (char *)getTable(tagSilf, &lSilf)) == NULL) return false;
    uint32 version;
    uint32 compilerVersion = 0; // wasn't set before GTF version 3
    uint32 offset32Pos = 2;
    version = swap32(*(uint32 *)pSilf);
    if (version < 0x00020000) return false;
    if (version >= 0x00030000)
    {
        compilerVersion = swap32(((uint32 *)pSilf)[1]);
        m_numSilf = swap16(((uint16 *)pSilf)[4]);
        offset32Pos = 3;
    }
    else
        m_numSilf = swap16(((uint16 *)pSilf)[2]);

#ifndef DISABLE_TRACING
        if (XmlTraceLog::get().active())
        {
            XmlTraceLog::get().openElement(ElementSilf);
            XmlTraceLog::get().addAttribute(AttrMajor, version >> 16);
            XmlTraceLog::get().addAttribute(AttrMinor, version & 0xFFFF);
            XmlTraceLog::get().addAttribute(AttrCompilerMajor, compilerVersion >> 16);
            XmlTraceLog::get().addAttribute(AttrCompilerMinor, compilerVersion & 0xFFFF);
            XmlTraceLog::get().addAttribute(AttrNum, m_numSilf);
            if (m_numSilf == 0)
                XmlTraceLog::get().warning("No Silf subtables!");
        }
#endif

    m_silfs = new Silf[m_numSilf];
    for (int i = 0; i < m_numSilf; i++)
    {
        const uint32 offset = swap32(((uint32 *)pSilf)[offset32Pos + i]);
        uint32 next;
        if (i == m_numSilf - 1)
            next = lSilf;
        else
            next = swap32(((uint32 *)pSilf)[offset32Pos + 1 + i]);
        if (offset > lSilf || next > lSilf)
        {
#ifndef DISABLE_TRACING
            XmlTraceLog::get().error("Invalid table %d offset %d length %u", i, offset, lSilf);
            XmlTraceLog::get().closeElement(ElementSilf);
#endif
            return false;
        }
        if (!m_silfs[i].readGraphite((void *)((char *)pSilf + offset), next - offset, m_pGlyphFaceCache->m_nGlyphs, version))
        {
#ifndef DISABLE_TRACING
            if (XmlTraceLog::get().active())
            {
                XmlTraceLog::get().error("Error reading Graphite subtable %d", i);
                XmlTraceLog::get().closeElement(ElementSilfSub); // for convenience
                XmlTraceLog::get().closeElement(ElementSilf);
            }
#endif
            return false;
        }
    }

#ifndef DISABLE_TRACING
    XmlTraceLog::get().closeElement(ElementSilf);
#endif
    return true;
}

void GrFace::runGraphite(Segment *seg, const Silf *aSilf) const
{
    VMScratch vms;

    aSilf->runGraphite(seg, this, &vms);
}

const Silf *GrFace::chooseSilf(uint32 script) const
{
    if (m_numSilf == 0)
        return NULL;
    else if (m_numSilf == 1 || script == 0)
        return m_silfs;
    else // do more work here
        return m_silfs;
}

uint16 GrFace::getGlyphMetric(uint16 gid, uint8 metric) const
{
    switch ((enum metrics)metric)
    {
        case kgmetAscent : return m_ascent;
        case kgmetDescent : return m_descent;
        default: return m_pGlyphFaceCache->glyph(gid)->getMetric(metric);
    }
}
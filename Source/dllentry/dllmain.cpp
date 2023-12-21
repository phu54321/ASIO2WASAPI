// Copyright (C) 2023 Hyunwoo Park
//
// This file is part of ASIO2WASAPI2.
//
// ASIO2WASAPI2 is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// ASIO2WASAPI2 is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with ASIO2WASAPI2.  If not, see <http://www.gnu.org/licenses/>.
//

#include "COMBaseClasses.h"
#include "../ASIO2WASAPI2.h"

static CFactoryTemplate s_Templates[1] = {
        {L"ASIO2WASAPI2", &CLSID_ASIO2WASAPI2_DRIVER, ASIO2WASAPI2::CreateInstance}};
static int s_cTemplates = sizeof(s_Templates) / sizeof(s_Templates[0]);

static void InitClasses(BOOL bLoading) {
    for (int i = 0; i < s_cTemplates; i++) {
        const CFactoryTemplate *pT = &s_Templates[i];
        if (pT->m_lpfnInit != nullptr) {
            (*pT->m_lpfnInit)(bLoading, pT->m_ClsID);
        }
    }
}

class CClassFactory : public IClassFactory {

private:
    const CFactoryTemplate *m_pTemplate;

    ULONG m_cRef;

    static int m_cLocked;

public:
    explicit CClassFactory(const CFactoryTemplate *);

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv) override;

    STDMETHODIMP_(ULONG) AddRef() override;

    STDMETHODIMP_(ULONG) Release() override;

    // IClassFactory
    STDMETHODIMP CreateInstance(LPUNKNOWN pUnkOuter, REFIID riid, void **pv) override;

    STDMETHODIMP LockServer(BOOL fLock) override;

    // allow DLLGetClassObject to know about global server lock status
    static BOOL IsLocked() {
        return (m_cLocked > 0);
    };
};

HINSTANCE g_hInstDLL;

//////////////

#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"

BOOL WINAPI DllMain(
        __in HINSTANCE hinstDLL,
        __in DWORD fdwReason,
        __in LPVOID) {
    switch (fdwReason) {

        case DLL_PROCESS_ATTACH:
            g_hInstDLL = hinstDLL;

            DisableThreadLibraryCalls(hinstDLL);
            InitClasses(TRUE);
            break;

        case DLL_PROCESS_DETACH:
            InitClasses(FALSE);
            break;

        default:;
    }
    return TRUE;
}

STDAPI DllGetClassObject(REFCLSID rClsID, REFIID riid, void **pv) {

    if (!(riid == IID_IUnknown) && !(riid == IID_IClassFactory)) {
        return E_NOINTERFACE;
    }

    // traverse the array of templates looking for one with this
    // class id
    for (int i = 0; i < s_cTemplates; i++) {
        const CFactoryTemplate *pT = &s_Templates[i];
        if (pT->IsClassID(rClsID)) {

            // found a template - make a class factory based on this
            // template

            *pv = (LPVOID) (LPUNKNOWN) new CClassFactory(pT);
            if (*pv == nullptr) {
                return E_OUTOFMEMORY;
            }
            ((LPUNKNOWN) *pv)->AddRef();
            return NOERROR;
        }
    }
    return CLASS_E_CLASSNOTAVAILABLE;
}

STDAPI DllCanUnloadNow() {
    if (CClassFactory::IsLocked() || CBaseObject::ObjectsActive()) {

        return S_FALSE;
    } else {
        return S_OK;
    }
}

#pragma clang diagnostic pop

////////////////////////////////

// process-wide dll locked state
int CClassFactory::m_cLocked = 0;

CClassFactory::CClassFactory(const CFactoryTemplate *pTemplate) {
    m_cRef = 0;
    m_pTemplate = pTemplate;
}

STDMETHODIMP CClassFactory::QueryInterface(REFIID riid, void **ppv) {
    *ppv = nullptr;

    // any interface on this object is the object pointer.
    if ((riid == IID_IUnknown) || (riid == IID_IClassFactory)) {
        *ppv = (LPVOID) this;
        // AddRef returned interface pointer
        ((LPUNKNOWN) *ppv)->AddRef();
        return NOERROR;
    }

    return ResultFromScode(E_NOINTERFACE);
}

STDMETHODIMP_(ULONG) CClassFactory::AddRef() {
    return ++m_cRef;
}

STDMETHODIMP_(ULONG) CClassFactory::Release() {
    ULONG rc;

    if (--m_cRef == 0) {
        delete this;
        rc = 0;
    } else
        rc = m_cRef;

    return rc;
}

STDMETHODIMP CClassFactory::CreateInstance(LPUNKNOWN pUnkOuter, REFIID riid, void **pv) {
    /* Enforce the normal OLE rules regarding interfaces and delegation */

    if (pUnkOuter != nullptr) {
        if (IsEqualIID(riid, IID_IUnknown) == FALSE) {
            return ResultFromScode(E_NOINTERFACE);
        }
    }

    /* Create the new object through the derived class's create function */

    HRESULT hr = NOERROR;
    CUnknown *pObj = m_pTemplate->CreateInstance(pUnkOuter, &hr);

    if (pObj == nullptr) {
        return E_OUTOFMEMORY;
    }

    /* Delete the object if we got a construction error */

    if (FAILED(hr)) {
        delete pObj;
        return hr;
    }

    /* Get a reference counted interface on the object */

    /* We wrap the non-delegating QI with NDAddRef & NDRelease. */
    /* This protects any outer object from being prematurely    */
    /* released by an inner object that may have to be created  */
    /* in order to supply the requested interface.              */
    pObj->NonDelegatingAddRef();
    hr = pObj->NonDelegatingQueryInterface(riid, pv);
    pObj->NonDelegatingRelease();
    /* Note that if NonDelegatingQueryInterface fails, it will  */
    /* not increment the ref count, so the NonDelegatingRelease */
    /* will drop the ref back to zero and the object will "self-*/
    /* destruct".  Hence we don't need additional tidy-up code  */
    /* to cope with NonDelegatingQueryInterface failing.        */

    return hr;
}

STDMETHODIMP CClassFactory::LockServer(BOOL fLock) {
    if (fLock) {
        m_cLocked++;
    } else {
        m_cLocked--;
    }
    return NOERROR;
}

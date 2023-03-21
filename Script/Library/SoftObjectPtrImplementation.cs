﻿using System.Runtime.CompilerServices;
using Script.Common;
using Script.CoreUObject;

namespace Script.Library
{
    public static class SoftObjectPtrImplementation
    {
        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void SoftObjectPtr_RegisterImplementation<T>(TSoftObjectPtr<T> InSoftObjectPtr, T InObject)
            where T : UObject;

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void SoftObjectPtr_UnRegisterImplementation<T>(TSoftObjectPtr<T> InSoftObjectPtr)
            where T : UObject;

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void SoftObjectPtr_GetImplementation<T>(TSoftObjectPtr<T> InSoftObjectPtr, out T OutValue)
            where T : UObject;
    }
}
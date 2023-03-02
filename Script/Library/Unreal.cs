﻿using Script.Common;
using Script.CoreUObject;

namespace Script.Library
{
    public static class Unreal
    {
        // @TODO
        public static T NewObject<T>(UObject Outer, UClass Class, FName Name) where T : UObject
        {
            UnrealImplementation.Unreal_NewObjectImplementation<T>(Outer, Class, Name, out var OutValue);

            return OutValue;
        }

        public static T NewObject<T>(UObject Outer) where T : UObject
        {
            UnrealImplementation.Unreal_NewObjectWithClassNameImplementation<T>(Outer, Utils.GetPathName(typeof(T)), "",
                out var OutValue);

            return OutValue;
        }

        // @TODO
        public static T NewObject<T>(UObject Outer, FName Name) where T : UObject
        {
            UnrealImplementation.Unreal_NewObjectWithClassNameImplementation<T>(Outer, Utils.GetPathName(typeof(T)),
                Name, out var OutValue);

            return OutValue;
        }
    }
}
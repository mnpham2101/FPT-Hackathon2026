# --- kotlinx.serialization ---
# Keep rules per https://github.com/Kotlin/kotlinx.serialization#android
# R8 full mode strips generic signatures and generated serializers unless kept.

-keepattributes *Annotation*, InnerClasses

# kotlinx-serialization-json relies on this internal field via reflection.
-keepclassmembers class kotlinx.serialization.json.** {
    *** Companion;
}
-keepclasseswithmembers class kotlinx.serialization.json.** {
    kotlinx.serialization.KSerializer serializer(...);
}

# Keep generated serializer() functions on @Serializable companion objects.
-if @kotlinx.serialization.Serializable class **
-keepclassmembers class <1> {
    static <1>$Companion Companion;
}
-if @kotlinx.serialization.Serializable class ** {
    static **$Companion Companion;
}
-keepclasseswithmembers class <2>$Companion {
    kotlinx.serialization.KSerializer serializer(...);
}

# Keep INSTANCE.serializer() on @Serializable objects.
-keepclassmembers @kotlinx.serialization.Serializable class ** {
    public static ** INSTANCE;
}
-if @kotlinx.serialization.Serializable class ** {
    public static ** INSTANCE;
}
-keepclassmembers class <1> {
    public static <1> INSTANCE;
    kotlinx.serialization.KSerializer serializer(...);
}

# Serialized R4 model classes (model layer, 4.5.1.1) must not be renamed —
# field names are the wire contract with the ADA ECU.
-keepclassmembers @kotlinx.serialization.Serializable class com.hackathon.v2x.ivi.model.** {
    <fields>;
}

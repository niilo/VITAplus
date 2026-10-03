package org.vita3k.emulator.data

import androidx.annotation.StringRes
import org.json.JSONObject
import org.vita3k.emulator.R

/**
 * The version of the running build, as the native layer reports it.
 *
 * A release build was made on a release tag, so [version] is the tag itself and
 * [isRelease] is true. Any other build reports the release it is based on and
 * how many commits past that tag it is, for example "v1.1-dev.63", and it
 * carries the UTC time it was built.
 */
data class AppVersion(
    val version: String,
    val baseVersion: String,
    val isRelease: Boolean,
    val commitsSinceRelease: Int,
    val buildDate: String,
    val versionCode: Long,
    val commitHash: String
) {
    /** Empty when the native layer returned nothing usable. */
    val isEmpty: Boolean get() = version.isBlank()

    /** One line for a subtitle, for example "v1.1-dev.63". */
    fun subtitle(): String = version

    /**
     * One line naming the build and the release it is based on, for example
     * "v1.1-dev.63 (based on v1.1)". Used in the update messages, where the
     * release has to be readable without the rest of the about sheet.
     */
    fun displayVersion(): String {
        if (isEmpty) {
            return ""
        }
        return if (isRelease || baseVersion.isBlank()) version else "$version (based on $baseVersion)"
    }

    /**
     * The lines for the about sheet. A release build shows the commit it was
     * made from. A development build also shows the release it is based on, the
     * commit count and the build date.
     */
    fun detailLines(): List<Pair<Detail, String>> = buildList {
        if (isEmpty) return@buildList

        if (!isRelease) {
            add(Detail.BASED_ON to baseVersion)
            add(Detail.COMMITS_AFTER_RELEASE to commitsSinceRelease.toString())
        }
        if (commitHash.isNotBlank()) {
            add(Detail.COMMIT to commitHash)
        }
        if (buildDate.isNotBlank()) {
            add(Detail.BUILD_DATE to buildDate)
        }
    }

    /** One labelled line of the about sheet. */
    enum class Detail(@StringRes val labelRes: Int) {
        BASED_ON(R.string.about_based_on),
        COMMITS_AFTER_RELEASE(R.string.about_commits_after_release),
        COMMIT(R.string.about_commit),
        BUILD_DATE(R.string.about_build_date)
    }

    companion object {
        /**
         * Reads the JSON that NativeLib.getAppVersion returns. Anything it
         * cannot read gives an empty version rather than an exception, because
         * a missing version must not stop the app list from loading.
         */
        fun parse(json: String): AppVersion {
            if (json.isBlank()) {
                return EMPTY
            }

            return try {
                val root = JSONObject(json)
                AppVersion(
                    version = root.optString("version"),
                    baseVersion = root.optString("baseVersion"),
                    isRelease = root.optBoolean("isRelease", false),
                    commitsSinceRelease = root.optInt("commitsSinceRelease", 0),
                    buildDate = root.optString("buildDate"),
                    versionCode = root.optLong("versionCode", 0L),
                    commitHash = root.optString("commitHash")
                )
            } catch (_: Exception) {
                EMPTY
            }
        }

        val EMPTY = AppVersion("", "", false, 0, "", 0L, "")
    }
}
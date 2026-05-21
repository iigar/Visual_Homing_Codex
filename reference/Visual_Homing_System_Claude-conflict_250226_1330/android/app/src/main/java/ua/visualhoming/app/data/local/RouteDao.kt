package ua.visualhoming.app.data.local

import androidx.room.*
import kotlinx.coroutines.flow.Flow

@Dao
interface RouteDao {
    @Query("SELECT * FROM routes ORDER BY createdAt DESC")
    fun getAllRoutes(): Flow<List<RouteEntity>>

    @Query("SELECT * FROM routes WHERE id = :id")
    suspend fun getRouteById(id: String): RouteEntity?

    @Insert(onConflict = OnConflictStrategy.REPLACE)
    suspend fun insertRoute(route: RouteEntity)

    @Delete
    suspend fun deleteRoute(route: RouteEntity)

    @Query("DELETE FROM routes WHERE id = :id")
    suspend fun deleteRouteById(id: String)

    @Query("SELECT * FROM routes WHERE isSynced = 0")
    suspend fun getUnsyncedRoutes(): List<RouteEntity>
}

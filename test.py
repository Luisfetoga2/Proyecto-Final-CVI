gridSize = [40,40,1]


grid = []
for i in range(gridSize[0]):
    grid.append([])
    for j in range(gridSize[1]):
        grid[i].append([])
        for k in range(gridSize[2]):
            grid[i][j].append([0,0,0,0])
            if j>=gridSize[1]/2-1 and j<=gridSize[1]/2:
                if i>=gridSize[0]/2-1 and i<=gridSize[0]/2:
                    grid[i][j][k] = [0,100,0,0]

import numpy as np

def advect(grid, timestep):
    grid = np.array(grid)
    gridSize = grid.shape[:3]
    gridSizeInv = [1.0 / s for s in gridSize]
    new_grid = np.zeros_like(grid)
    for i in range(gridSize[0]):
        for j in range(gridSize[1]):
            for k in range(gridSize[2]):
                pos = np.array([i, j, k], dtype=np.float32)
                vel = grid[i, j, k, :3]
                pos_prev = pos - timestep * vel
                # Only add 0.5 if gridSize[2] > 1 (3D case)
                if gridSize[2] > 1:
                    pos_prev[2] += 0.5
                uvw = pos_prev * gridSizeInv
                prev_pos_grid = uvw * (np.array(gridSize) - 1)
                i0, j0, k0 = np.floor(prev_pos_grid).astype(int)
                i1, j1, k1 = np.ceil(prev_pos_grid).astype(int)
                xd, yd, zd = prev_pos_grid - [i0, j0, k0]
                def get_val(x, y, z):
                    if 0 <= x < gridSize[0] and 0 <= y < gridSize[1] and 0 <= z < gridSize[2]:
                        return grid[x, y, z, :3]
                    else:
                        return np.array([0.0, 0.0, 0.0])
                c000 = get_val(i0, j0, k0)
                c100 = get_val(i1, j0, k0)
                c010 = get_val(i0, j1, k0)
                c110 = get_val(i1, j1, k0)
                c001 = get_val(i0, j0, k1)
                c101 = get_val(i1, j0, k1)
                c011 = get_val(i0, j1, k1)
                c111 = get_val(i1, j1, k1)
                c00 = c000 * (1 - xd) + c100 * xd
                c01 = c001 * (1 - xd) + c101 * xd
                c10 = c010 * (1 - xd) + c110 * xd
                c11 = c011 * (1 - xd) + c111 * xd
                c0 = c00 * (1 - yd) + c10 * yd
                c1 = c01 * (1 - yd) + c11 * yd
                interp = c0 * (1 - zd) + c1 * zd
                new_grid[i, j, k, :3] = interp
    return new_grid.tolist()

# Test the advect function
import time
import pygame

pygame.init()
screen = pygame.display.set_mode((600, 600))
clock = pygame.time.Clock()
running = True

speed = 0
while running:
    for event in pygame.event.get():
        if event.type == pygame.QUIT:
            running = False

    # Update the grid
    dt = clock.get_time() / 1000.0 * speed
    grid = advect(grid, dt)

    # Clear the screen
    screen.fill((0, 0, 0))

    # Draw the grid
    for i in range(gridSize[0]):
        for j in range(gridSize[1]):
            for k in range(gridSize[2]):
                color = (int(grid[i][j][k][0] * 255), int(grid[i][j][k][1] * 255), int(grid[i][j][k][2] * 255))
                color = (max(0, min(255, color[0])),
                         max(0, min(255, color[1])),
                         max(0, min(255, color[2])))
                pygame.draw.rect(screen, color, (i * 150, j * 150, 150, 150))

    pygame.display.flip()
    clock.tick(60)